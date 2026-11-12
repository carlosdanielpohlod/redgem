// Navigation on grids drawn by hand: paths found and pulled straight, ledges and gaps, the budget
// a search keeps to, the nearest walkable cell and whether a point stands on one.

#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "navigation/nav_grid.hpp"
#include "navigation/nav_path.hpp"
#include "navigation/nav_queries.hpp"

using namespace redgem;

namespace {

constexpr float kCell = 1.0f;
constexpr float kMaxStep = 0.4f;
constexpr float kStepHeight = 0.3f;   // climbed
constexpr float kLedgeHeight = 1.0f;  // not climbed
constexpr float kHeadroom = 1.8f;
constexpr float kTolerance = 1e-4f;
constexpr char kGround = '.';
constexpr char kStep = '-';
constexpr char kLedge = '^';
constexpr char kNothing = '#';

std::vector<float> column_from(char mark) {
  if (mark == kGround) return {0.0f};
  if (mark == kStep) return {kStepHeight};
  if (mark == kLedge) return {kLedgeHeight};
  return {};
}

// A grid drawn row by row (z), a mark a column (x): '.' ground, '-' a step up, '^' a ledge higher
// than a step, '#' nothing to stand on.
NavGrid grid_from(const std::vector<std::string>& rows) {
  NavGridShape shape;
  shape.cell_size = kCell;
  shape.columns = static_cast<int>(rows.front().size());
  shape.rows = static_cast<int>(rows.size());
  shape.max_step = kMaxStep;
  std::vector<std::vector<float>> heights;
  for (const std::string& row : rows) {
    for (const char mark : row) heights.push_back(column_from(mark));
  }
  return NavGrid(shape, heights);
}

NavGrid open_ground(int columns, int rows) {
  return grid_from(std::vector<std::string>(static_cast<std::size_t>(rows), std::string(columns, kGround)));
}

NavGrid::Node node_at(const NavGrid& grid, int x, int z) {
  const auto column = grid.column_of_cell({x, z});
  REQUIRE(column.has_value());
  const auto [first, last] = grid.nodes_in(*column);
  REQUIRE(first < last);
  return first;
}

bool side_by_side(const NavGrid& grid, NavGrid::Node a, NavGrid::Node b) {
  const NavCell one = grid.cell_of_column(grid.column_of(a));
  const NavCell other = grid.cell_of_column(grid.column_of(b));
  return std::abs(one.x - other.x) <= 1 && std::abs(one.z - other.z) <= 1;
}

bool walks_every_piece(const NavGrid& grid, const std::vector<NavGrid::Node>& nodes) {
  for (std::size_t index = 1; index < nodes.size(); ++index) {
    if (!walks_straight(grid, nodes[index - 1], nodes[index])) return false;
  }
  return true;
}

bool passes_column(const NavGrid& grid, const std::vector<NavGrid::Node>& nodes, int x, int z) {
  for (const NavGrid::Node node : nodes) {
    if (grid.column_of(node) == *grid.column_of_cell({x, z})) return true;
  }
  return false;
}

// A wall across the middle with a gap at its far end.
const std::vector<std::string> kWallWithGap = {
    "....#....",  //
    "....#....",  //
    "....#....",  //
    "....#....",  //
    ".........",  //
};

}  // namespace

TEST_CASE("a path on open ground goes cell by cell from the start to the goal") {
  const NavGrid grid = open_ground(10, 10);
  const NavGridPath path = find_nav_path(grid, node_at(grid, 0, 0), node_at(grid, 9, 6));
  REQUIRE(path.status == NavPathStatus::Found);
  CHECK(path.nodes.front() == node_at(grid, 0, 0));
  CHECK(path.nodes.back() == node_at(grid, 9, 6));
  CHECK(path.nodes.size() == 10);  // diagonals while both axes go, then straight on
  for (std::size_t index = 1; index < path.nodes.size(); ++index) {
    CHECK(side_by_side(grid, path.nodes[index - 1], path.nodes[index]));
  }
}

TEST_CASE("a path goes around a wall through its gap") {
  const NavGrid grid = grid_from(kWallWithGap);
  const NavGridPath path = find_nav_path(grid, node_at(grid, 0, 0), node_at(grid, 8, 0));
  REQUIRE(path.status == NavPathStatus::Found);
  CHECK(passes_column(grid, path.nodes, 4, 4));
}

TEST_CASE("a path pulled straight is shorter, with fewer points, and still walks") {
  const NavGrid grid = open_ground(20, 20);
  const NavGridPath path = find_nav_path(grid, node_at(grid, 0, 0), node_at(grid, 19, 7));
  REQUIRE(path.status == NavPathStatus::Found);
  const std::vector<NavGrid::Node> pulled = pull_straight(grid, path.nodes);
  CHECK(pulled.size() == 2);  // nothing in the way: one straight line
  CHECK(nav_path_length(grid, pulled) < nav_path_length(grid, path.nodes) - kCell);
  CHECK(pulled.front() == path.nodes.front());
  CHECK(pulled.back() == path.nodes.back());
}

TEST_CASE("a path pulled straight keeps the corners it turns around") {
  const NavGrid grid = grid_from(kWallWithGap);
  const NavGridPath path = find_nav_path(grid, node_at(grid, 0, 0), node_at(grid, 8, 0));
  REQUIRE(path.status == NavPathStatus::Found);
  const std::vector<NavGrid::Node> pulled = pull_straight(grid, path.nodes);
  CHECK(pulled.size() >= 3);
  CHECK(pulled.size() < path.nodes.size());
  CHECK(walks_every_piece(grid, pulled));
  CHECK(nav_path_length(grid, pulled) <= nav_path_length(grid, path.nodes));
}

TEST_CASE("a step is climbed, a ledge higher than a step is not") {
  const NavGrid stepped = grid_from({"..-..", "..-..", "..-.."});
  CHECK(find_nav_path(stepped, node_at(stepped, 0, 1), node_at(stepped, 4, 1)).status == NavPathStatus::Found);
  const NavGrid ledged = grid_from({"..^..", "..^..", "..^.."});
  CHECK(find_nav_path(ledged, node_at(ledged, 0, 1), node_at(ledged, 4, 1)).status == NavPathStatus::Unreachable);
  CHECK(find_nav_path(ledged, node_at(ledged, 2, 0), node_at(ledged, 2, 2)).status == NavPathStatus::Found);
}

TEST_CASE("a goal across a gap cannot be reached") {
  const NavGrid grid = grid_from({"..##..", "..##..", "..##.."});
  const NavGridPath path = find_nav_path(grid, node_at(grid, 0, 0), node_at(grid, 5, 2));
  CHECK(path.status == NavPathStatus::Unreachable);
  CHECK(path.nodes.empty());
  CHECK(path.searched == 6);  // every cell of its own side, and no more
}

TEST_CASE("a search stops at its budget") {
  const NavGrid grid = open_ground(200, 200);
  constexpr std::size_t kSmallBudget = 100;
  const NavGridPath stopped = find_nav_path(grid, node_at(grid, 0, 0), node_at(grid, 199, 199), kSmallBudget);
  CHECK(stopped.status == NavPathStatus::OutOfBudget);
  CHECK(stopped.searched == kSmallBudget);
  CHECK(stopped.nodes.empty());
  const NavGridPath found = find_nav_path(grid, node_at(grid, 0, 0), node_at(grid, 199, 199));
  CHECK(found.status == NavPathStatus::Found);
  CHECK(found.searched <= nav::kMaxSearchNodes);
}

TEST_CASE("cells over each other are told apart: a bridge over a road") {
  NavGridShape shape;
  shape.cell_size = kCell;
  shape.columns = 3;
  shape.rows = 1;
  shape.max_step = kMaxStep;
  constexpr float kBridge = 3.0f;
  const NavGrid grid(shape, {{kBridge, 0.0f}, {kBridge, 0.0f}, {kBridge, 0.0f}});
  CHECK(grid.node_count() == 6);
  const auto [first, last] = grid.nodes_in(1);
  CHECK(last - first == 2);
  CHECK(grid.step_to(node_at(grid, 0, 0), 1) == first);          // on the bridge
  CHECK(grid.step_to(node_at(grid, 0, 0) + 1, 1) == first + 1);  // on the road under it
}

TEST_CASE("the nearest walkable point of a point") {
  const NavGrid grid = grid_from({"...", ".#.", "..."});
  const auto in_hole = nearest_nav_node(grid, {1.5f, 0.0f, 1.5f});
  REQUIRE(in_hole.has_value());
  CHECK(length(grid.position(*in_hole) - Vec3{1.5f, 0.0f, 1.5f}) == doctest::Approx(kCell).epsilon(kTolerance));
  const auto above = nearest_nav_node(grid, {0.5f, 3.0f, 0.5f});
  REQUIRE(above.has_value());
  CHECK(*above == node_at(grid, 0, 0));
  CHECK_FALSE(nearest_nav_node(grid, {1000.0f, 0.0f, 0.0f}).has_value());  // beyond reach
  CHECK_FALSE(nearest_nav_node(NavGrid(), {0.0f, 0.0f, 0.0f}).has_value());
}

TEST_CASE("a point is on walkable ground over a walkable cell, up to a character's height above it") {
  const NavGrid grid = grid_from({"...", ".#.", "..."});
  CHECK(on_nav_grid(grid, {0.5f, 0.0f, 0.5f}, kHeadroom));
  CHECK(on_nav_grid(grid, {0.5f, 1.0f, 0.5f}, kHeadroom));
  CHECK_FALSE(on_nav_grid(grid, {0.5f, 3.0f, 0.5f}, kHeadroom));
  CHECK_FALSE(on_nav_grid(grid, {0.5f, -1.0f, 0.5f}, kHeadroom));
  CHECK_FALSE(on_nav_grid(grid, {1.5f, 0.0f, 1.5f}, kHeadroom));   // the hole
  CHECK_FALSE(on_nav_grid(grid, {-5.0f, 0.0f, 0.5f}, kHeadroom));  // off the grid
}
