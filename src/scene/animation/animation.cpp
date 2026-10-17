#include "scene/animation/animation.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr std::size_t kVectorComponents = 3;
constexpr std::size_t kQuaternionComponents = 4;

std::size_t components_of(AnimationPath path) {
  return path == AnimationPath::Rotation ? kQuaternionComponents : kVectorComponents;
}

// The two keys around a time, and how far from the first to the second that time is (0 to 1).
struct KeyPair {
  std::size_t previous = 0;
  std::size_t next = 0;
  float blend = 0.0f;
};

KeyPair keys_around(const std::vector<float>& times, std::size_t keys, float time) {
  const auto after = std::upper_bound(times.begin(), times.begin() + static_cast<std::ptrdiff_t>(keys), time);
  KeyPair pair;
  pair.next = std::min(static_cast<std::size_t>(after - times.begin()), keys - 1);
  pair.previous = pair.next == 0 ? 0 : pair.next - 1;
  const float span = times[pair.next] - times[pair.previous];
  pair.blend = span > 0.0f ? std::clamp((time - times[pair.previous]) / span, 0.0f, 1.0f) : 0.0f;
  return pair;
}

// Quaternions q and -q are the same rotation: -1 when one of two must be flipped so that a blend
// between them takes the short way.
float short_way_sign(float alignment) { return alignment < 0.0f ? -1.0f : 1.0f; }

// `blend` of the way from `a` to `b` (taken as `b * sign`), float by float.
ChannelValue blended(const float* a, const float* b, float sign, float blend, std::size_t width) {
  ChannelValue out{};
  for (std::size_t i = 0; i < width; ++i) out[i] = a[i] + (b[i] * sign - a[i]) * blend;
  return out;
}

// Scaled to length 1 (all zeros stay so).
ChannelValue normalized(ChannelValue value) {
  float length_squared = 0.0f;
  for (const float component : value) length_squared += component * component;
  const float scale = length_squared > 0.0f ? 1.0f / std::sqrt(length_squared) : 0.0f;
  for (float& component : value) component *= scale;
  return value;
}

ChannelValue blended_rotation(const float* a, const float* b, float blend) {
  const float alignment =
      a[kXOffset] * b[kXOffset] + a[kYOffset] * b[kYOffset] + a[kZOffset] * b[kZOffset] + a[kWOffset] * b[kWOffset];
  return normalized(blended(a, b, short_way_sign(alignment), blend, kQuaternionComponents));
}

}  // namespace

ChannelValue sample_channel(const AnimationChannel& channel, float time) {
  const std::size_t width = components_of(channel.path);
  const std::size_t keys = std::min(channel.times.size(), channel.values.size() / width);
  if (keys == 0) return ChannelValue{};
  // The two keys around `time`.
  const KeyPair around = keys_around(channel.times, keys, time);
  const float* a = &channel.values[around.previous * width];
  const float* b = &channel.values[around.next * width];
  if (channel.path == AnimationPath::Rotation) return blended_rotation(a, b, around.blend);
  return blended(a, b, 1.0f, around.blend, width);
}

namespace {

using redgem::mix;  // (points; rotations below)

// The rotation `weight` of the way from one to the other, the short way round.
Quat mix(Quat from, Quat to, float weight) {
  const float sign = short_way_sign(from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w);
  return normalize(Quat{from.x + (to.x * sign - from.x) * weight, from.y + (to.y * sign - from.y) * weight,
                        from.z + (to.z * sign - from.z) * weight, from.w + (to.w * sign - from.w) * weight});
}

// A clip's pose with all its weight replaces what is there; with less it moves it that share of
// the way.
template <typename Value>
Value laid_over(Value current, Value to, float weight) {
  return weight >= 1.0f ? to : mix(current, to, weight);
}

// Where in a clip `time` seconds of playing it is.
float clip_time(const AnimationDesc& clip, float time, bool loop) {
  if (clip.duration <= 0.0f) return 0.0f;
  if (loop) return std::fmod(std::max(time, 0.0f), clip.duration);
  return std::clamp(time, 0.0f, clip.duration);
}

// A clip being laid over a scene at one moment.
struct Playback {
  const AnimationDesc& clip;
  float time = 0.0f;
  const PoseOptions& options;

  bool wants(ObjectId object) const { return !options.wanted || options.wanted(object); }
};

void pose(const AnimationChannel& channel, const Playback& playback, float time, Transform& local) {
  const float weight = playback.options.weight;
  const ChannelValue value = sample_channel(channel, clip_time(playback.clip, time, playback.options.loop));
  switch (channel.path) {
    case AnimationPath::Translation:
      local.position = laid_over(local.position, vec3_from(value.data()), weight);
      break;
    case AnimationPath::Rotation:
      local.rotation = laid_over(local.rotation, quat_from(value.data()), weight);
      break;
    case AnimationPath::Scale:
      local.scale = laid_over(local.scale, vec3_from(value.data()), weight);
      break;
  }
}

void pose_object(const AnimationChannel& channel, const Playback& playback, float time, GameObject& object) {
  if (playback.options.before) playback.options.before(object.id, channel.path, object.local);
  pose(channel, playback, time, object.local);
}

void pose_original(const AnimationChannel& channel, const Playback& playback, Scene& scene) {
  const bool posed = scene.is_alive(channel.node) && playback.wants(channel.node);
  if (!posed) return;
  pose_object(channel, playback, playback.time, scene.at(channel.node));
}

// The root of a copy stays where it was put; what is inside it moves like the original.
bool moves_like_the_original(const GameObject& copy, const Playback& playback) {
  return copy.copy_root != copy.id && playback.wants(copy.id);
}

void pose_copies(const AnimationChannel& channel, const Playback& playback, Scene& scene) {
  for (const ObjectId copy : scene.copies_of(channel.node)) {
    GameObject& object = scene.at(copy);
    if (!moves_like_the_original(object, playback)) continue;
    const float ahead = playback.options.offsets ? scene.at(object.copy_root).animation_offset : 0.0f;
    pose_object(channel, playback, playback.time + ahead, object);
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
  const Playback playback{clip, time, options};
  for (const AnimationChannel& channel : clip.channels) {
    pose_original(channel, playback, scene);
    pose_copies(channel, playback, scene);
  }
}

}  // namespace redgem
