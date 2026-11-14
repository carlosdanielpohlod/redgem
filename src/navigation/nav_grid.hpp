#pragma once

// Where characters can stand: a 2.5D grid over the level seen from above. Each column of the grid
// (a square `cell_size` a side) holds the heights of the surfaces in it a character fits on, top
// first, so a bridge over a road gives two cells in the same column. Each of those cells is a node
// of the graph paths are found in: two cells side by side (diagonals included) are joined when
// their heights are no more than `max_step` apart.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "core/math.hpp"

namespace redgem {

// Where a column is: its place along x and along z.
struct NavCell {
  int x = 0;
  int z = 0;
};

// The size and the place of a grid: its corner of least x and z, its cells, and the highest step
// a character climbs from one to the next.
struct NavGridShape {
  float origin_x = 0.0f;
  float origin_z = 0.0f;
  float cell_size = 1.0f;
  int columns = 0;  // along x
  int rows = 0;     // along z
  float max_step = 0.0f;

  int column_count() const { return columns * rows; }
  NavCell cell_of(int column) const { return {column % columns, column / columns}; }
  // The middle of a cell's column along x, and along z.
  float center_x(int x) const { return origin_x + (static_cast<float>(x) + kHalf) * cell_size; }
  float center_z(int z) const { return origin_z + (static_cast<float>(z) + kHalf) * cell_size; }
};

class NavGrid {
 public:
  using Node = std::uint32_t;
  static constexpr std::size_t kMaxNeighbours = 8;
  // A node next to another, and how far apart they are.
  struct Link {
    Node node = 0;
    float cost = 0.0f;
  };
  using Links = std::array<Link, kMaxNeighbours>;

  NavGrid() = default;
  // `heights` holds the walkable heights of each column (row by row along z, x along each row).
  NavGrid(const NavGridShape& shape, const std::vector<std::vector<float>>& heights);

  const NavGridShape& shape() const { return shape_; }
  std::size_t node_count() const { return heights_.size(); }
  bool empty() const { return heights_.empty(); }

  // A node's place: the middle of its column, at its height.
  Vec3 position(Node node) const;
  float height(Node node) const { return heights_[node]; }
  int column_of(Node node) const { return static_cast<int>(node_columns_[node]); }
  NavCell cell_of_column(int column) const { return shape_.cell_of(column); }
  // The column a point is over, or none outside the grid.
  std::optional<int> column_at(float x, float z) const;
  // The column of a cell, or none outside the grid.
  std::optional<int> column_of_cell(NavCell cell) const;
  // The cell a point is over, kept to the grid's edges for a point outside it.
  NavCell nearest_cell(float x, float z) const;
  // The nodes of a column: [first, last).
  std::pair<Node, Node> nodes_in(int column) const;
  // The node of a column a character on `from` steps onto (the one nearest its height), if any.
  std::optional<Node> step_to(Node from, int column) const;
  // Whether a character on `from` steps onto `to`, a node of a column beside its own.
  bool joins(Node from, Node to) const { return step_to(from, column_of(to)) == to; }
  // The nodes a character on `node` walks to in one step; returns how many there are. A diagonal
  // step is one only when both straight steps beside it are, and both of those lead to it too: no
  // path cuts a corner.
  std::size_t neighbours(Node node, Links& links) const;

 private:
  float step_cost(Node from, Node to) const;

  NavGridShape shape_;
  std::vector<std::uint32_t> column_starts_;  // column_count() + 1 entries
  std::vector<float> heights_;                // a node's height
  std::vector<std::uint32_t> node_columns_;   // a node's column
};

}  // namespace redgem
