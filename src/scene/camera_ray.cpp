#include "scene/camera_ray.hpp"

#include <cmath>

#include "scene/scene_config.hpp"

namespace redgem {
namespace {

constexpr float kFullSpan = 2.0f;  // a screen spans -1..1
constexpr float kHalfSpan = 1.0f;

}  // namespace

Vec3 screen_ray_direction(const Transform& camera_world, float fov_y_radians, const ScreenPoint& point) {
  const float fov = fov_y_radians > 0.0f ? fov_y_radians : degrees_to_radians(config::kDefaultFovYDegrees);
  const float tan_half = std::tan(fov * kHalf);
  const float aspect = static_cast<float>(point.screen_width) / static_cast<float>(point.screen_height);
  // From pixels to -1..1 across the screen, up positive.
  const float across = (point.x / static_cast<float>(point.screen_width)) * kFullSpan - kHalfSpan;
  const float upward = kHalfSpan - (point.y / static_cast<float>(point.screen_height)) * kFullSpan;
  return normalize(camera_world.forward() + camera_world.right() * (across * tan_half * aspect) +
                   camera_world.up() * (upward * tan_half));
}

}  // namespace redgem
