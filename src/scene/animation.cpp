#include "scene/animation.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr std::size_t kVectorComponents = 3;
constexpr std::size_t kQuaternionComponents = 4;

std::size_t components_of(AnimationPath path) {
  return path == AnimationPath::Rotation ? kQuaternionComponents : kVectorComponents;
}

}  // namespace

std::array<float, 4> sample_channel(const AnimationChannel& channel, float time) {
  std::array<float, 4> out{};
  const std::size_t width = components_of(channel.path);
  const std::size_t keys = std::min(channel.times.size(), channel.values.size() / width);
  if (keys == 0) return out;
  // The two keys around `time`.
  const auto after = std::upper_bound(channel.times.begin(), channel.times.begin() + static_cast<std::ptrdiff_t>(keys), time);
  const std::size_t next = std::min(static_cast<std::size_t>(after - channel.times.begin()), keys - 1);
  const std::size_t previous = next == 0 ? 0 : next - 1;
  const float span = channel.times[next] - channel.times[previous];
  const float blend = span > 0.0f ? std::clamp((time - channel.times[previous]) / span, 0.0f, 1.0f) : 0.0f;
  const float* a = &channel.values[previous * width];
  const float* b = &channel.values[next * width];
  if (channel.path != AnimationPath::Rotation) {
    for (std::size_t i = 0; i < width; ++i) out[i] = a[i] + (b[i] - a[i]) * blend;
    return out;
  }
  // Quaternions q and -q are the same rotation: flip one so the blend takes the short way.
  const float alignment = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  const float sign = alignment < 0.0f ? -1.0f : 1.0f;
  float length_squared = 0.0f;
  for (std::size_t i = 0; i < width; ++i) {
    out[i] = a[i] + (b[i] * sign - a[i]) * blend;
    length_squared += out[i] * out[i];
  }
  const float scale = length_squared > 0.0f ? 1.0f / std::sqrt(length_squared) : 0.0f;
  for (std::size_t i = 0; i < width; ++i) out[i] *= scale;
  return out;
}

namespace {

void pose(const AnimationChannel& channel, const AnimationDesc& clip, float time, bool loop, Transform& local) {
  const float local_time = clip.duration <= 0.0f ? 0.0f
                           : loop                ? std::fmod(std::max(time, 0.0f), clip.duration)
                                                 : std::clamp(time, 0.0f, clip.duration);
  const std::array<float, 4> value = sample_channel(channel, local_time);
  switch (channel.path) {
    case AnimationPath::Translation: local.position = {value[0], value[1], value[2]}; break;
    case AnimationPath::Rotation: local.rotation = Quat{value[0], value[1], value[2], value[3]}; break;
    case AnimationPath::Scale: local.scale = {value[0], value[1], value[2]}; break;
  }
}

}  // namespace

void apply_animation(const AnimationDesc& clip, float time, Scene& scene, bool loop) {
  for (const AnimationChannel& channel : clip.channels) {
    if (scene.is_alive(channel.node)) pose(channel, clip, time, loop, scene.at(channel.node).local);
    for (const ObjectId copy : scene.copies_of(channel.node)) {
      GameObject& object = scene.at(copy);
      // The root of a copy stays where it was put; what is inside it moves like the original.
      if (object.copy_root == copy) continue;
      pose(channel, clip, time + scene.at(object.copy_root).animation_offset, loop, object.local);
    }
  }
}

}  // namespace redgem
