#pragma once

#include "scene/transform.hpp"

namespace redgem {

// A point of the screen, in pixels from its top left corner, and the size of that screen.
struct ScreenPoint {
  float x = 0.0f;
  float y = 0.0f;
  int screen_width = 0;
  int screen_height = 0;
};

// The direction (normalized) of the ray that leaves a camera through a point of the screen.
// `fov_y_radians` is the camera's field of view from top to bottom; the engine's default when it
// is not positive.
Vec3 screen_ray_direction(const Transform& camera_world, float fov_y_radians, const ScreenPoint& point);

}  // namespace redgem
