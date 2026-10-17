#pragma once

// How scattered copies (scene/scatter.hpp) are judged by a view: tens of thousands are too many to
// judge one by one every pass, so they are cut into square cells of the ground, and a pass draws
// or skips a whole cell. No window and no graphics card needed, so it can be tested.

#include <cstddef>
#include <vector>

#include "render/instance_batches.hpp"
#include "render/material_alpha.hpp"
#include "render/visibility.hpp"
#include "scene/scatter.hpp"
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

// Whether a view may draw a cell: in view, and its biggest copy big enough from the cell's
// nearest point.
bool chunk_worth_drawing(const View& view, const ScatterChunk& chunk);

// How much of a cell lies within [from, to) meters of a view: a cell across those distances is
// judged copy by copy, so that the sets of two levels of detail never draw the same place twice.
enum class Reach { None, Some, All };
Reach reach_of(const View& view, const ScatterChunk& chunk, float from, float to);

// A set of scattered copies as it is drawn: its mesh, the distances it is drawn at, its cells,
// and the center of each copy's sphere, by record from `first` on.
struct ScatterLayout {
  int mesh = kNoIndex;
  float visible_from = 0.0f;
  float visible_to = 0.0f;
  bool cast_shadow = true;
  std::size_t first = 0;
  std::vector<ScatterChunk> chunks;
  std::vector<Vec3> centers;
};
// Lays a set's copies out from record `first` on, a mesh with `bounds`. Fills `order` as
// chunk_copies does.
ScatterLayout lay_out_copies(const ScatterSet& set, const Bounds& bounds, std::size_t first,
                             std::vector<std::size_t>& order);
// Adds to `batches` the copies of a set the view draws: whole cells, and the copies of a cell
// across the distances the set is drawn at that are within them.
void gather_set(InstanceBatches& batches, const View& view, const ScatterLayout& layout);

// Copies of one mesh a pass gathered: `count` record numbers from `first` on in what it gathered
// (InstanceBatches::slots). The pass's record numbers start at `pass_first` among the frame's
// (what SeeThroughDraw::first counts).
struct GatheredRun {
  int mesh = kNoIndex;
  std::size_t first = 0;
  std::size_t count = 0;
  std::size_t pass_first = 0;
};
// Adds to `draws` the see-through parts of scattered copies, cell by cell, each cell at the
// distance of its middle: sorted among the objects, at a draw call a cell rather than one a copy.
// (The copies of a cell are not sorted among themselves.)
void add_see_through_cells(std::vector<SeeThroughDraw>& draws, const std::vector<float>& records,
                           const std::vector<ScatterLayout>& layouts, const GatheredRun& run, Vec3 viewer);

}  // namespace redgem::render_math
