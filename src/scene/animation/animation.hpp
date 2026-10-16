#pragma once

#include <array>
#include <functional>

#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// A channel's value: 3 floats for translation and scale, 4 for a rotation (the last one unused
// otherwise).
inline constexpr std::size_t kMaxChannelFloats = 4;
using ChannelValue = std::array<float, kMaxChannelFloats>;

// The value of a channel at `time` seconds (clamped to its first and last keys): 3 floats for
// translation and scale, a normalized quaternion (x y z w) for rotation. Keys are interpolated
// linearly (rotations along the shortest arc).
ChannelValue sample_channel(const AnimationChannel& channel, float time);

// Poses the nodes a clip animates as they are `time` seconds into it, looping. Nodes that no
// longer exist are skipped. Copies of those nodes (Scene::clone) are posed too, each as far
// ahead in the clip as the animation_offset of its copy's root says.
// Without `loop`, times past the end hold the clip's last pose.
void apply_animation(const AnimationDesc& clip, float time, Scene& scene, bool loop = true);

// How a clip is laid over the poses objects already have.
struct PoseOptions {
  bool loop = true;
  // 1 replaces the pose; less moves it that share of the way (blending two clips, or fading
  // from one to the next).
  float weight = 1.0f;
  // Whether copies run ahead by their root's animation_offset.
  bool offsets = true;
  // Which objects to pose; all of the clip's when empty.
  std::function<bool(ObjectId)> wanted;
  // Called before each channel is laid on an object, with what it is about to change (the
  // player uses it to give blends a steady pose to start from).
  std::function<void(ObjectId, AnimationPath, Transform&)> before;
};
void apply_animation(const AnimationDesc& clip, float time, Scene& scene, const PoseOptions& options);

}  // namespace redgem
