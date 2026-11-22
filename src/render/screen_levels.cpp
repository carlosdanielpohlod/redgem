#include "render/screen_levels.hpp"

#include <algorithm>
#include <cmath>

namespace redgem::render_math {
namespace {

bool has_level(const ScreenLevel& level, float tan_half_fov) { return level.radius > 0.0f && tan_half_fov > 0.0f; }

// How far a sphere of `radius` is when it covers `size` of the view's height.
float distance_at(float size, float radius, float tan_half_fov) { return radius / (size * tan_half_fov); }

}  // namespace

DrawnRange drawn_range(const GameObject& object, float scale, float tan_half_fov) {
  DrawnRange range{object.visible_from, object.visible_to};
  const ScreenLevel& level = object.screen_level;
  if (!has_level(level, tan_half_fov)) return range;
  const float radius = level.radius * scale;
  range.fade = level.fade * radius / tan_half_fov;
  if (level.from > 0.0f) range.to = std::min(range.to, distance_at(level.from, radius, tan_half_fov));
  if (std::isfinite(level.to)) {
    range.from = std::max(range.from, distance_at(level.to, radius, tan_half_fov) - range.fade);
  }
  return range;
}

float longest_axis(Vec3 scale) { return std::max({std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z)}); }

}  // namespace redgem::render_math
