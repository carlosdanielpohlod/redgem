#pragma once

// Baking: making the grid characters walk on (navigation/nav_grid.hpp) out of what the level is.
// The level is asked through a probe, two questions a column at a time: what surfaces are there,
// and would a character standing on one of them be inside something. (nav_physics_probe.hpp asks
// the physics world; tests can ask anything.)
//
// A surface is walkable when it is no steeper than the steepest slope, a character standing on it
// is inside nothing (walls within its radius, ceilings within its height, anything taller than a
// step), and it is more than a step above the walkable surface found above it in its column.
//
// The work is bounded: a grid has at most kMaxBakeColumns columns (a larger level is baked with
// larger cells) and a column at most kMaxColumnLayers walkable cells.

#include <cstddef>
#include <vector>

#include "core/math.hpp"
#include "navigation/nav_grid.hpp"
#include "navigation/nav_settings.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

namespace nav {
inline constexpr int kMaxBakeColumns = 512 * 512;
inline constexpr std::size_t kMaxColumnLayers = 8;
}  // namespace nav

// A surface a ray down a column met: its height and which way it faces.
struct NavSurface {
  float height = 0.0f;
  Vec3 normal = Vec3::up();
};

class NavProbe {
 public:
  virtual ~NavProbe() = default;
  // What a ray straight down at (x, z) meets, from the top down.
  virtual std::vector<NavSurface> surfaces(float x, float z) const = 0;
  // Whether a character standing with its feet there would be inside something.
  virtual bool blocked(Vec3 feet) const = 0;
};

// How a bake went: how big its grid is, and how long it took.
struct NavBakeReport {
  int columns = 0;
  std::size_t cells = 0;   // walkable cells: the nodes of the graph
  float cell_size = 0.0f;  // larger than asked when the level was too big for the grid
  bool cells_enlarged = false;
  double milliseconds = 0.0;
};

// The grid over `area` (its x and z: the y of a level is the probe's business), with the cell size
// of the settings, or a larger one so that the grid has at most kMaxBakeColumns columns.
NavGridShape nav_grid_shape(const Bounds& area, const NavSettings& settings);

NavGrid bake_nav_grid(const NavGridShape& shape, const NavSettings& settings, const NavProbe& probe);

}  // namespace redgem
