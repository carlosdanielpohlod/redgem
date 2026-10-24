#pragma once

// What objects made in the editor start as: those of the GameObject menu, and those a new scene
// starts with (see editor/game_scenes.hpp).

#include "core/math.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::new_objects {

inline constexpr const char* kSunName = "Sun";
inline constexpr const char* kCameraName = "Camera";
inline constexpr Color kGrassColor{0.33f, 0.46f, 0.25f, 1.0f};  // new terrains and grounds
inline constexpr Color kLightColor{1.0f, 1.0f, 1.0f, 1.0f};     // white
inline constexpr float kSunIntensity = 3.0f;                    // W/m2, like Blender's sun strength
// New suns shine down at an angle instead of straight along -Z.
inline constexpr Vec3 kSunDirection{-0.4f, -1.0f, -0.6f};
inline constexpr float kCameraFov = 0.87f;  // ~50 degrees

// How a new sun is turned: looking along kSunDirection.
inline Quat sun_rotation() { return quat_look_rotation(normalize(kSunDirection), Vec3::up()); }

}  // namespace redgem::new_objects
