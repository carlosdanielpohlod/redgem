#pragma once

// How scattered copies (scene/scatter.hpp) are judged by a view: tens of thousands are too many to
// judge one by one every pass, so they are cut into square cells of the ground, and a pass draws
// or skips a whole cell. No window and no graphics card needed, so it can be tested.

#include <cstddef>
#include <vector>

#include "render/visibility.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem::render_math {

inline constexpr float kScatterCell = 16.0f;  // meters across a cell

// The copies of one cell: `count` records in a row from `first` on.
struct ScatterChunk {
  std::size_t first = 0;
  std::size_t count = 0;
  BoundingSphere sphere;      // around every copy of the cell
  float largest_copy = 0.0f;  // the radius of the biggest one, meters
};

// Cuts copies of a mesh with `bounds` into cells. Fills `order` with the copies' numbers, cell by
// cell: their records are written in that order, `first` counting from the set's first record.
std::vector<ScatterChunk> chunk_copies(const std::vector<Transform>& places, const Bounds& bounds,
                                       std::vector<std::size_t>& order);

// Whether a view draws a cell: in view, its biggest copy big enough from the nearest point of the
// cell, and part of the cell within [from, to) meters.
bool chunk_worth_drawing(const View& view, const ScatterChunk& chunk, float from, float to);

}  // namespace redgem::render_math
