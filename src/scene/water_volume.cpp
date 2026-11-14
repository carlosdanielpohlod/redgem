#include "scene/water_volume.hpp"

#include <cmath>

namespace redgem {
namespace {

// Whether a surface at `height` over the column is above the water found there so far.
bool higher(float height, const std::optional<WaterColumn>& found) { return !found || height > found->height; }

// Whether a surface at `height` is nearer a point at `y` than the one found so far.
bool nearer(float height, const std::optional<WaterColumn>& found, float y) {
  return !found || std::fabs(height - y) < std::fabs(found->height - y);
}

void keep_nearer(std::optional<WaterColumn>& found, WaterColumn candidate, float y) {
  if (nearer(candidate.height, found, y)) found = candidate;
}

}  // namespace

std::optional<WaterColumn> water_column_over(const std::vector<WaterSurface>& surfaces, Vec3 point) {
  std::optional<WaterColumn> above;
  std::optional<WaterColumn> below;
  for (const WaterSurface& surface : surfaces) {
    const auto height = water_surface_height(surface, point.x, point.z);
    if (!height) continue;
    keep_nearer(*height >= point.y ? above : below, WaterColumn{*height, &surface}, point.y);
  }
  return above ? above : below;
}

std::optional<WaterColumn> water_column_at(const std::vector<WaterSurface>& surfaces, float x, float z) {
  std::optional<WaterColumn> found;
  for (const WaterSurface& surface : surfaces) {
    const auto height = water_surface_height(surface, x, z);
    if (!height) continue;
    if (higher(*height, found)) found = WaterColumn{*height, &surface};
  }
  return found;
}

std::optional<float> water_depth(const std::vector<WaterSurface>& surfaces, Vec3 point) {
  if (!finite_water_point(point)) return std::nullopt;
  const auto column = water_column_over(surfaces, point);
  if (!column) return std::nullopt;
  return column->height - point.y;
}

bool water_under(const std::vector<WaterSurface>& surfaces, Vec3 point) {
  const auto depth = water_depth(surfaces, point);
  return depth && *depth > 0.0f;
}

}  // namespace redgem
