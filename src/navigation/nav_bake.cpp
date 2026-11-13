#include "navigation/nav_bake.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

// How much larger a cell is made each time a grid still has too many columns.
constexpr float kCellGrowth = 1.05f;

// What a surface has to be to be stood on.
struct SurfaceRules {
  float min_up = 0.0f;  // the least its normal points up: the cosine of the steepest slope
  float max_step = 0.0f;
};

int cells_across(float span, float cell_size) { return std::max(1, static_cast<int>(std::ceil(span / cell_size))); }

long long columns_for(float width, float depth, float cell_size) {
  return static_cast<long long>(cells_across(width, cell_size)) * cells_across(depth, cell_size);
}

// The smallest cell, from the settings' up, that covers the level with few enough columns.
float fitting_cell_size(float width, float depth, float asked) {
  const auto most = static_cast<float>(nav::kMaxBakeColumns);
  float cell_size = std::max({asked, std::sqrt(width * depth / most), std::max(width, depth) / most});
  while (columns_for(width, depth, cell_size) > nav::kMaxBakeColumns) cell_size *= kCellGrowth;
  return cell_size;
}

bool steep(const NavSurface& surface, const SurfaceRules& rules) { return surface.normal.y < rules.min_up; }

// Whether a surface is one already kept (the top of a box lying on another, at the same height).
bool already_kept(const NavSurface& surface, const SurfaceRules& rules, const std::vector<float>& kept) {
  return !kept.empty() && std::fabs(kept.back() - surface.height) <= rules.max_step;
}

bool unfit(const NavSurface& surface, const SurfaceRules& rules, const std::vector<float>& kept) {
  return steep(surface, rules) || already_kept(surface, rules, kept);
}

// The heights a character stands at in the column at (x, z), top first.
std::vector<float> walkable_heights(const NavProbe& probe, float x, float z, const SurfaceRules& rules) {
  std::vector<float> kept;
  for (const NavSurface& surface : probe.surfaces(x, z)) {
    if (kept.size() == nav::kMaxColumnLayers) break;
    if (unfit(surface, rules, kept)) continue;
    if (!probe.blocked({x, surface.height, z})) kept.push_back(surface.height);
  }
  return kept;
}

}  // namespace

NavGridShape nav_grid_shape(const Bounds& area, const NavSettings& settings) {
  NavGridShape shape;
  shape.max_step = settings.max_step;
  shape.cell_size = settings.cell_size;
  if (!area.valid) return shape;
  const float width = std::max(area.max.x - area.min.x, 0.0f);
  const float depth = std::max(area.max.z - area.min.z, 0.0f);
  shape.cell_size = fitting_cell_size(width, depth, settings.cell_size);
  shape.origin_x = area.min.x;
  shape.origin_z = area.min.z;
  shape.columns = cells_across(width, shape.cell_size);
  shape.rows = cells_across(depth, shape.cell_size);
  return shape;
}

NavGrid bake_nav_grid(const NavGridShape& shape, const NavSettings& settings, const NavProbe& probe) {
  const SurfaceRules rules{std::cos(degrees_to_radians(settings.max_slope_degrees)), settings.max_step};
  std::vector<std::vector<float>> heights(static_cast<std::size_t>(std::max(shape.column_count(), 0)));
  for (std::size_t column = 0; column < heights.size(); ++column) {
    const NavCell cell = shape.cell_of(static_cast<int>(column));
    heights[column] = walkable_heights(probe, shape.center_x(cell.x), shape.center_z(cell.z), rules);
  }
  return NavGrid(shape, heights);
}

}  // namespace redgem
