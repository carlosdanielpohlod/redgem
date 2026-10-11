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

Vec3 mix(Vec3 from, Vec3 to, float weight) { return from + (to - from) * weight; }

// The rotation `weight` of the way from one to the other, the short way round.
Quat mix(Quat from, Quat to, float weight) {
  const float alignment = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
  const float sign = alignment < 0.0f ? -1.0f : 1.0f;
  return normalize(Quat{from.x + (to.x * sign - from.x) * weight, from.y + (to.y * sign - from.y) * weight,
                        from.z + (to.z * sign - from.z) * weight, from.w + (to.w * sign - from.w) * weight});
}

void pose(const AnimationChannel& channel, const AnimationDesc& clip, float time, const PoseOptions& options,
          Transform& local) {
  const float local_time = clip.duration <= 0.0f ? 0.0f
                           : options.loop        ? std::fmod(std::max(time, 0.0f), clip.duration)
                                                 : std::clamp(time, 0.0f, clip.duration);
  const std::array<float, 4> value = sample_channel(channel, local_time);
  const bool whole = options.weight >= 1.0f;
  switch (channel.path) {
    case AnimationPath::Translation: {
      const Vec3 to{value[0], value[1], value[2]};
      local.position = whole ? to : mix(local.position, to, options.weight);
      break;
    }
    case AnimationPath::Rotation: {
      const Quat to{value[0], value[1], value[2], value[3]};
      local.rotation = whole ? to : mix(local.rotation, to, options.weight);
      break;
    }
    case AnimationPath::Scale: {
      const Vec3 to{value[0], value[1], value[2]};
      local.scale = whole ? to : mix(local.scale, to, options.weight);
      break;
    }
  }
}

}  // namespace

void apply_animation(const AnimationDesc& clip, float time, Scene& scene, bool loop) {
  PoseOptions options;
  options.loop = loop;
  apply_animation(clip, time, scene, options);
}

void apply_animation(const AnimationDesc& clip, float time, Scene& scene, const PoseOptions& options) {
  if (options.weight <= 0.0f) return;
  const auto wanted = [&options](ObjectId object) { return !options.wanted || options.wanted(object); };
  for (const AnimationChannel& channel : clip.channels) {
    if (scene.is_alive(channel.node) && wanted(channel.node)) {
      Transform& local = scene.at(channel.node).local;
      if (options.before) options.before(channel.node, channel.path, local);
      pose(channel, clip, time, options, local);
    }
    for (const ObjectId copy : scene.copies_of(channel.node)) {
      GameObject& object = scene.at(copy);
      // The root of a copy stays where it was put; what is inside it moves like the original.
      if (object.copy_root == copy || !wanted(copy)) continue;
      const float ahead = options.offsets ? scene.at(object.copy_root).animation_offset : 0.0f;
      if (options.before) options.before(copy, channel.path, object.local);
      pose(channel, clip, time + ahead, options, object.local);
    }
  }
}

}  // namespace redgem
