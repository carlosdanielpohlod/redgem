#pragma once

// Paths across a grid (navigation/nav_grid.hpp): the shortest chain of cells from one to another
// (A*), then pulled straight (string pulling: every cell a straight walk can skip is left out).
//
// A search is bounded: it looks at no more than `budget` cells, so that no script stalls a frame
// asking for a path across a world. One that runs out says so, as one that finds no way does.
//
// A search may be told to go around cells it would walk otherwise (NavBlocked): those under the
// bodies that move, as a character found them in its way.

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "core/math.hpp"
#include "navigation/nav_grid.hpp"

namespace redgem {

namespace nav {
// The most cells one search looks at.
inline constexpr std::size_t kMaxSearchNodes = 16384;
// The most cells one straight piece of a pulled path crosses: longer straights get a waypoint in
// between, which keeps pulling a path linear in its length.
inline constexpr std::size_t kMaxStraightCells = 64;
}  // namespace nav

// Something in the way that the grid does not know of (a body that moves): a box seen from above
// as a disc, from `bottom` to `top`.
struct NavObstacle {
  Vec3 center;
  float radius = 0.0f;
  float bottom = 0.0f;
  float top = 0.0f;
};

// How far into an obstacle's reach a character standing on a point (`clearance` wide, `headroom`
// tall) is, in meters: 0 or less when it is clear of it, or under it, or over it.
float nav_obstacle_depth(Vec3 point, const NavObstacle& obstacle, float clearance, float headroom);

// The cells of a grid a search keeps off: those where a character standing (`clearance` wide,
// `headroom` tall) would overlap an obstacle. Each is as deep in as it is near the obstacle's
// middle. A walk may step off one onto a shallower one (a character that stands next to what is
// in its way first walks away from it), never deeper in.
class NavBlocked {
 public:
  NavBlocked() = default;
  NavBlocked(const NavGrid& grid, const std::vector<NavObstacle>& obstacles, float clearance, float headroom);
  // Blocks nothing.
  static const NavBlocked& none();

  bool blocks(NavGrid::Node node) const { return depths_.count(node) != 0; }
  std::size_t size() const { return depths_.size(); }
  // Whether a step from one node onto another is let through: onto an open one, or out of the way.
  bool lets(NavGrid::Node from, NavGrid::Node to) const;

 private:
  void block_under(const NavGrid& grid, const NavObstacle& obstacle, float clearance, float headroom);
  void block_column(const NavGrid& grid, int column, const NavObstacle& obstacle, float clearance, float headroom);

  float depth(NavGrid::Node node) const;

  std::unordered_map<NavGrid::Node, float> depths_;  // meters into the nearest obstacle's reach
};

enum class NavPathStatus { Found, Unreachable, OutOfBudget };

struct NavGridPath {
  NavPathStatus status = NavPathStatus::Unreachable;
  std::vector<NavGrid::Node> nodes;  // from the start to the goal, both included
  std::size_t searched = 0;          // cells looked at
};

// The goal is never blocked: a character may be sent next to what is in the way.
NavGridPath find_nav_path(const NavGrid& grid, NavGrid::Node start, NavGrid::Node goal,
                          std::size_t budget = nav::kMaxSearchNodes, const NavBlocked& blocked = NavBlocked::none());

// Whether a character walks straight from one node to another without leaving walkable cells or
// climbing more than a step between two of them, nor going deeper into blocked ones (but onto `to`).
bool walks_straight(const NavGrid& grid, NavGrid::Node from, NavGrid::Node to,
                    const NavBlocked& blocked = NavBlocked::none());

// The path with every node a straight walk can skip left out: its first and last nodes are kept.
std::vector<NavGrid::Node> pull_straight(const NavGrid& grid, const std::vector<NavGrid::Node>& nodes,
                                         const NavBlocked& blocked = NavBlocked::none());

// How long a path is, in meters.
float nav_path_length(const NavGrid& grid, const std::vector<NavGrid::Node>& nodes);

}  // namespace redgem
