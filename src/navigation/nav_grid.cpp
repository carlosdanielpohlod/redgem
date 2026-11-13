#include "navigation/nav_grid.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

// A step to a column beside: how far along x and z.
struct Offset {
  int dx = 0;
  int dz = 0;
};

constexpr std::size_t kStraightSteps = 4;
constexpr std::array<Offset, kStraightSteps> kStraight = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

// A diagonal step, and the two straight steps (indices into kStraight) it goes between: it is
// only taken when both are, so that a path never cuts a corner.
struct Diagonal {
  Offset offset;
  std::size_t along_x = 0;
  std::size_t along_z = 0;
};
constexpr std::size_t kDiagonalSteps = 4;
constexpr std::size_t kEast = 0;
constexpr std::size_t kWest = 1;
constexpr std::size_t kNorth = 2;
constexpr std::size_t kSouth = 3;
constexpr std::array<Diagonal, kDiagonalSteps> kDiagonals = {{
    {{1, 1}, kEast, kNorth},
    {{1, -1}, kEast, kSouth},
    {{-1, 1}, kWest, kNorth},
    {{-1, -1}, kWest, kSouth},
}};

// Whether a place along an axis, in cells, is one of the `count` cells of the grid.
bool inside_span(float index, int count) { return index >= 0.0f && index < static_cast<float>(count); }

bool inside_grid(NavCell cell, const NavGridShape& shape) {
  return cell.x >= 0 && cell.x < shape.columns && cell.z >= 0 && cell.z < shape.rows;
}

// Whether a point, in cells from the grid's corner, is over the grid.
bool over_grid(float cell_x, float cell_z, const NavGridShape& shape) {
  return inside_span(cell_x, shape.columns) && inside_span(cell_z, shape.rows);
}

using Node = NavGrid::Node;
// The nodes the straight steps from a node lead to, in kStraight's order.
using Sides = std::array<std::optional<Node>, kStraightSteps>;

// The node a character on `node` steps onto in the column at `offset` from its cell, if any.
std::optional<Node> step_by(const NavGrid& grid, Node node, NavCell cell, Offset offset) {
  const std::optional<int> column = grid.column_of_cell({cell.x + offset.dx, cell.z + offset.dz});
  if (!column) return std::nullopt;
  return grid.step_to(node, *column);
}

// Whether a node is a step from both of two others.
bool joined_from_both(const NavGrid& grid, Node one, Node other, Node node) {
  return grid.joins(one, node) && grid.joins(other, node);
}

// The node across a corner, when both straight steps beside it (`sides`) can be taken and both
// lead there too.
std::optional<Node> across_corner(const NavGrid& grid, Node node, NavCell cell, const Diagonal& diagonal,
                                  const Sides& sides) {
  const std::optional<Node>& beside_x = sides[diagonal.along_x];
  const std::optional<Node>& beside_z = sides[diagonal.along_z];
  if (!beside_x) return std::nullopt;
  if (!beside_z) return std::nullopt;
  const std::optional<Node> next = step_by(grid, node, cell, diagonal.offset);
  if (!next) return std::nullopt;
  if (!joined_from_both(grid, *beside_x, *beside_z, *next)) return std::nullopt;
  return next;
}

// A place along an axis, in cells, as the index of the nearest cell of the `count` there are (a
// place far off the grid, or none at all, gives an edge's).
int clamped_index(float index, int count) {
  const auto last = static_cast<float>(std::max(count - 1, 0));
  // (fmax and fmin, unlike max and min, take a number over a NaN.)
  return static_cast<int>(std::fmin(std::fmax(std::floor(index), 0.0f), last));
}

}  // namespace

NavGrid::NavGrid(const NavGridShape& shape, const std::vector<std::vector<float>>& heights) : shape_(shape) {
  const auto columns = static_cast<std::size_t>(std::max(shape.column_count(), 0));
  column_starts_.reserve(columns + 1);
  for (std::size_t column = 0; column < columns; ++column) {
    column_starts_.push_back(static_cast<std::uint32_t>(heights_.size()));
    if (column >= heights.size()) continue;
    heights_.insert(heights_.end(), heights[column].begin(), heights[column].end());
    node_columns_.resize(heights_.size(), static_cast<std::uint32_t>(column));
  }
  column_starts_.push_back(static_cast<std::uint32_t>(heights_.size()));
}

Vec3 NavGrid::position(Node node) const {
  const NavCell cell = cell_of_column(column_of(node));
  return {shape_.center_x(cell.x), heights_[node], shape_.center_z(cell.z)};
}

std::optional<int> NavGrid::column_at(float x, float z) const {
  const float cell_x = (x - shape_.origin_x) / shape_.cell_size;
  const float cell_z = (z - shape_.origin_z) / shape_.cell_size;
  if (!over_grid(cell_x, cell_z, shape_)) return std::nullopt;
  return column_of_cell({static_cast<int>(cell_x), static_cast<int>(cell_z)});
}

std::optional<int> NavGrid::column_of_cell(NavCell cell) const {
  if (!inside_grid(cell, shape_)) return std::nullopt;
  return cell.z * shape_.columns + cell.x;
}

NavCell NavGrid::nearest_cell(float x, float z) const {
  return {clamped_index((x - shape_.origin_x) / shape_.cell_size, shape_.columns),
          clamped_index((z - shape_.origin_z) / shape_.cell_size, shape_.rows)};
}

std::pair<NavGrid::Node, NavGrid::Node> NavGrid::nodes_in(int column) const {
  const auto index = static_cast<std::size_t>(column);
  return {column_starts_[index], column_starts_[index + 1]};
}

std::optional<NavGrid::Node> NavGrid::step_to(Node from, int column) const {
  const auto [first, last] = nodes_in(column);
  std::optional<Node> best;
  float best_rise = shape_.max_step;
  for (Node node = first; node < last; ++node) {
    const float rise = std::fabs(heights_[node] - heights_[from]);
    if (rise > best_rise) continue;
    best = node;
    best_rise = rise;
  }
  return best;
}

float NavGrid::step_cost(Node from, Node to) const { return length(position(to) - position(from)); }

std::size_t NavGrid::neighbours(Node node, Links& links) const {
  const NavCell cell = cell_of_column(column_of(node));
  std::size_t count = 0;
  Sides sides;
  for (std::size_t step = 0; step < kStraightSteps; ++step) {
    const std::optional<Node> side = step_by(*this, node, cell, kStraight[step]);
    sides[step] = side;
    if (side) links[count++] = {*side, step_cost(node, *side)};
  }
  for (const Diagonal& diagonal : kDiagonals) {
    const std::optional<Node> next = across_corner(*this, node, cell, diagonal, sides);
    if (next) links[count++] = {*next, step_cost(node, *next)};
  }
  return count;
}

}  // namespace redgem
