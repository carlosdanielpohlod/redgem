#pragma once

// Paths across a grid (navigation/nav_grid.hpp): the shortest chain of cells from one to another
// (A*), then pulled straight (string pulling: every cell a straight walk can skip is left out).
//
// A search is bounded: it looks at no more than `budget` cells, so that no script stalls a frame
// asking for a path across a world. One that runs out says so, as one that finds no way does.

#include <cstddef>
#include <vector>

#include "navigation/nav_grid.hpp"

namespace redgem {

namespace nav {
// The most cells one search looks at.
inline constexpr std::size_t kMaxSearchNodes = 16384;
// The most cells one straight piece of a pulled path crosses: longer straights get a waypoint in
// between, which keeps pulling a path linear in its length.
inline constexpr std::size_t kMaxStraightCells = 64;
}  // namespace nav

enum class NavPathStatus { Found, Unreachable, OutOfBudget };

struct NavGridPath {
  NavPathStatus status = NavPathStatus::Unreachable;
  std::vector<NavGrid::Node> nodes;  // from the start to the goal, both included
  std::size_t searched = 0;          // cells looked at
};

NavGridPath find_nav_path(const NavGrid& grid, NavGrid::Node start, NavGrid::Node goal,
                          std::size_t budget = nav::kMaxSearchNodes);

// Whether a character walks straight from one node to another without leaving walkable cells or
// climbing more than a step between two of them.
bool walks_straight(const NavGrid& grid, NavGrid::Node from, NavGrid::Node to);

// The path with every node a straight walk can skip left out: its first and last nodes are kept.
std::vector<NavGrid::Node> pull_straight(const NavGrid& grid, const std::vector<NavGrid::Node>& nodes);

// How long a path is, in meters.
float nav_path_length(const NavGrid& grid, const std::vector<NavGrid::Node>& nodes);

}  // namespace redgem
