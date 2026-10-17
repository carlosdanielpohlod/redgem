#pragma once

// A character for tests of animation between machines: an object "Swordsman" holding a "Sword",
// and two clips that move the sword: "Attack" (along x, a meter in a second) and "Guard" (along
// y, a meter in two seconds).

#include "scene/scene_desc.hpp"

namespace redgem::test {

inline constexpr const char* kSwordsman = "Swordsman";
inline constexpr float kAttackSeconds = 1.0f;
inline constexpr float kSwordReach = 1.0f;  // meters the sword moves in a clip

// Adds the swordsman and his clips to a scene.
void add_swordsman(SceneDesc& desc);

}  // namespace redgem::test
