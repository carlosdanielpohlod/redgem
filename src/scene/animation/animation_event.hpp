#pragma once

// What a character's own animation hands on after each advance (AnimationPlayer): the moments
// of its clips it went past, for the scripts listening, and how far its root moved, for the
// object to follow.

#include <cstdint>
#include <string>

#include "core/math.hpp"
#include "scene/scene.hpp"

namespace redgem {

// Listener ids are never 0: 0 says that nothing listens (an unknown clip).
inline constexpr std::uint32_t kNoAnimationListener = 0;

struct AnimationEvent {
  ObjectHandle object;  // who listens: the object whose animation.on asked
  std::uint32_t listener = kNoAnimationListener;
  std::string clip;
  std::string name;  // the authored marker's; empty for a moment a script named
  float at = 0.0f;   // normalized: 0 is the clip's start, 1 its end
};

// How far a character's own clips moved its root in one advance, to move the character by.
struct RootMotion {
  ObjectHandle character;
  Vec3 translation;  // world space, horizontal
  float yaw = 0.0f;  // radians around the world's up axis
};

}  // namespace redgem
