#pragma once

#include <array>

#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// The value of a channel at `time` seconds (clamped to its first and last keys): 3 floats for
// translation and scale, a normalized quaternion (x y z w) for rotation. Keys are interpolated
// linearly (rotations along the shortest arc).
std::array<float, 4> sample_channel(const AnimationChannel& channel, float time);

// Poses the nodes a clip animates as they are `time` seconds into it, looping. Nodes that no
// longer exist are skipped. Copies of those nodes (Scene::clone) are posed too, each as far
// ahead in the clip as the animation_offset of its copy's root says.
void apply_animation(const AnimationDesc& clip, float time, Scene& scene);

}  // namespace redgem
