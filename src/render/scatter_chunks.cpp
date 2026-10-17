#include "render/scatter_chunks.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

#include "render/culling.hpp"

namespace redgem::render_math {
namespace {

using Cell = std::pair<int, int>;  // which cell, along x and along z

Cell cell_of(Vec3 position) {
  return {static_cast<int>(std::floor(position.x / kScatterCell)),
          static_cast<int>(std::floor(position.z / kScatterCell))};
}

// The copies `order[first..last)`, all of one cell, as a chunk.
ScatterChunk chunk_of(const std::vector<std::size_t>& order, const std::vector<BoundingSphere>& spheres,
                      std::size_t first, std::size_t last) {
  Bounds box;
  ScatterChunk chunk{first, last - first, {}, 0.0f};
  for (std::size_t at = first; at < last; ++at) {
    const BoundingSphere& copy = spheres[order[at]];
    box.include(copy.center - Vec3::one() * copy.radius);
    box.include(copy.center + Vec3::one() * copy.radius);
    chunk.largest_copy = std::max(chunk.largest_copy, copy.radius);
  }
  chunk.sphere = {box.center(), length(box.max - box.min) * kHalf};
  return chunk;
}

// Where the run of copies of `order[first]`'s cell ends.
std::size_t end_of_cell(const std::vector<std::size_t>& order, const std::vector<Cell>& cells, std::size_t first) {
  std::size_t last = first + 1;
  while (last < order.size() && cells[order[last]] == cells[order[first]]) ++last;
  return last;
}

// Whether no part of a sphere `distance` meters away lies within [from, to).
bool outside_range(float distance, float radius, float from, float to) {
  return distance - radius >= to || distance + radius < from;
}

}  // namespace

std::vector<ScatterChunk> chunk_copies(const std::vector<Transform>& places, const Bounds& bounds,
                                       std::vector<std::size_t>& order) {
  std::vector<BoundingSphere> spheres;
  std::vector<Cell> cells;
  for (const Transform& place : places) {
    spheres.push_back(world_sphere(bounds, place));
    cells.push_back(cell_of(place.position));
  }
  order.resize(places.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(), [&cells](std::size_t a, std::size_t b) { return cells[a] < cells[b]; });
  std::vector<ScatterChunk> chunks;
  for (std::size_t first = 0; first < order.size(); first = chunks.back().first + chunks.back().count) {
    chunks.push_back(chunk_of(order, spheres, first, end_of_cell(order, cells, first)));
  }
  return chunks;
}

bool chunk_worth_drawing(const View& view, const ScatterChunk& chunk, float from, float to) {
  if (!sphere_in_frustum(view.frustum, chunk.sphere.center, chunk.sphere.radius)) return false;
  const float distance = length(chunk.sphere.center - view.viewer);
  if (outside_range(distance, chunk.sphere.radius, from, to)) return false;
  // Too small to see even from the nearest point of the cell (radius over distance, as for objects).
  const float nearest = std::max(distance - chunk.sphere.radius, 0.0f);
  return chunk.largest_copy >= view.min_angle * nearest;
}

}  // namespace redgem::render_math
