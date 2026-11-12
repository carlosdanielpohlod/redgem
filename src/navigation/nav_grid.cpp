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

// Whether both straight steps a diagonal goes between can be taken.
bool corner_open(const std::array<bool, kStraightSteps>& open, const Diagonal& diagonal) {
  return open[diagonal.along_x] && open[diagonal.along_z];
}

int clamped_index(float index, int count) {
  return std::clamp(static_cast<int>(std::floor(index)), 0, std::max(count - 1, 0));
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

std::optional<NavGrid::Node> NavGrid::neighbour(Node node, NavCell cell, int dx, int dz) const {
  const std::optional<int> column = column_of_cell({cell.x + dx, cell.z + dz});
  if (!column) return std::nullopt;
  return step_to(node, *column);
}

float NavGrid::step_cost(Node from, Node to) const { return length(position(to) - position(from)); }

std::size_t NavGrid::neighbours(Node node, Links& links) const {
  const NavCell cell = cell_of_column(column_of(node));
  std::size_t count = 0;
  std::array<bool, kStraightSteps> open{};
  for (std::size_t step = 0; step < kStraightSteps; ++step) {
    const std::optional<Node> next = neighbour(node, cell, kStraight[step].dx, kStraight[step].dz);
    open[step] = next.has_value();
    if (next) links[count++] = {*next, step_cost(node, *next)};
  }
  for (const Diagonal& diagonal : kDiagonals) {
    if (!corner_open(open, diagonal)) continue;
    const std::optional<Node> next = neighbour(node, cell, diagonal.offset.dx, diagonal.offset.dz);
    if (next) links[count++] = {*next, step_cost(node, *next)};
  }
  return count;
}

}  // namespace redgem
