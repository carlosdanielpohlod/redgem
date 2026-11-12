#pragma once

// Questions about single points of a grid (navigation/nav_grid.hpp): the walkable cell nearest a
// point, and whether a point stands on one.

#include <optional>

#include "core/math.hpp"
#include "navigation/nav_grid.hpp"
#include "navigation/nav_settings.hpp"

namespace redgem {

namespace nav {
// How far around a point its nearest walkable cell is looked for, in meters: enough rings of
// columns around the point's own for the smallest cells to reach it.
inline constexpr float kNearestReach = 8.0f;
inline constexpr int kMaxNearestRings = static_cast<int>(kNearestReach / kMinCellSize) + 1;
}  // namespace nav

// The walkable cell nearest a point (straight-line distance), within nav::kNearestReach of it.
std::optional<NavGrid::Node> nearest_nav_node(const NavGrid& grid, Vec3 point);

// Whether a point is on a walkable cell: over one, at most `headroom` above it and at most a step
// below it.
bool on_nav_grid(const NavGrid& grid, Vec3 point, float headroom);

}  // namespace redgem
