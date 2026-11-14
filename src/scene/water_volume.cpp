#include "scene/water_volume.hpp"

namespace redgem {
namespace {

// Whether a surface at `height` over the column is above the water found there so far.
bool higher(const std::optional<float>& height, const std::optional<WaterColumn>& found) {
  if (!height) return false;
  return !found || *height > found->height;
}

}  // namespace

std::optional<WaterColumn> water_column_at(const std::vector<WaterSurface>& surfaces, float x, float z) {
  std::optional<WaterColumn> found;
  for (const WaterSurface& surface : surfaces) {
    const auto height = water_surface_height(surface, x, z);
    if (higher(height, found)) found = WaterColumn{*height, &surface};
  }
  return found;
}

std::optional<float> water_depth(const std::vector<WaterSurface>& surfaces, Vec3 point) {
  if (!finite_water_point(point)) return std::nullopt;
  const auto column = water_column_at(surfaces, point.x, point.z);
  if (!column) return std::nullopt;
  return column->height - point.y;
}

bool water_under(const std::vector<WaterSurface>& surfaces, Vec3 point) {
  const auto depth = water_depth(surfaces, point);
  return depth && *depth > 0.0f;
}

}  // namespace redgem
