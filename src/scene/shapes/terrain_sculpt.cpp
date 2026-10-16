// Brush strokes on a terrain.

#include <algorithm>
#include <cmath>

#include "scene/shapes/terrain.hpp"

namespace redgem {
namespace {

constexpr float kFullWeight = 1.0f;
constexpr float kNeighbourCount = 4.0f;

// Brush strength at a distance from its center: 1 in the middle, easing to 0 at the radius.
float falloff(float distance, float radius) {
  if (distance >= radius) return 0.0f;
  return kHalf * (kFullWeight + std::cos(kPi * distance / radius));
}

// The rows (or columns) a brush may reach, both ends included.
struct GridSpan {
  int first = 0;
  int last = 0;
};

GridSpan span_under(const TerrainGrid& grid, float center, float radius) {
  const float half = grid.size * kHalf;
  return {std::max(static_cast<int>(std::floor((center - radius + half) / grid.spacing())), 0),
          std::min(static_cast<int>(std::ceil((center + radius + half) / grid.spacing())), grid.cells)};
}

float neighbour_average(const PrimitiveDesc& mesh, const TerrainGrid& grid, int column, int row) {
  return (vertex_height(mesh, grid, column - 1, row) + vertex_height(mesh, grid, column + 1, row) +
          vertex_height(mesh, grid, column, row - 1) + vertex_height(mesh, grid, column, row + 1)) /
         kNeighbourCount;
}

// What a stroke knows besides its brush.
struct Stroke {
  const TerrainBrush& brush;
  float center_height = 0.0f;   // the ground under the brush's center, which Flatten pulls towards
  const PrimitiveDesc& before;  // the heights before the stroke, which Smooth reads
};

// A vertex of the grid as a stroke finds it.
struct GridVertex {
  int column = 0;
  int row = 0;
  float height = 0.0f;
};

// How much a stroke moves a vertex that it reaches with `weight`.
float height_change(const Stroke& stroke, const TerrainGrid& grid, const GridVertex& vertex, float weight) {
  const float push = stroke.brush.amount * weight;
  const float share = std::clamp(push, 0.0f, kFullWeight);
  switch (stroke.brush.mode) {
    case SculptMode::Raise:
      return push;
    case SculptMode::Lower:
      return -push;
    case SculptMode::Flatten:
      return (stroke.center_height - vertex.height) * share;
    case SculptMode::Smooth:
      return (neighbour_average(stroke.before, grid, vertex.column, vertex.row) - vertex.height) * share;
  }
  return 0.0f;
}

// Returns false when the stroke does not reach the vertex.
bool sculpt_vertex(PrimitiveDesc& mesh, const TerrainGrid& grid, const Stroke& stroke, int column, int row) {
  const float across = grid.coordinate(column) - stroke.brush.x;
  const float along = grid.coordinate(row) - stroke.brush.z;
  const float weight = falloff(std::hypot(across, along), stroke.brush.radius);
  if (weight <= 0.0f) return false;
  float& height = mesh.positions[grid.vertex_offset(column, row) + kYOffset];
  height += height_change(stroke, grid, {column, row, height}, weight);
  return true;
}

// Sculpts every vertex the stroke may reach. Returns whether it reached any.
bool sculpt_reached(PrimitiveDesc& mesh, const TerrainGrid& grid, const Stroke& stroke) {
  const GridSpan columns = span_under(grid, stroke.brush.x, stroke.brush.radius);
  const GridSpan rows = span_under(grid, stroke.brush.z, stroke.brush.radius);
  bool touched = false;
  for (int row = rows.first; row <= rows.last; ++row) {
    for (int column = columns.first; column <= columns.last; ++column) {
      touched = sculpt_vertex(mesh, grid, stroke, column, row) || touched;
    }
  }
  return touched;
}

}  // namespace

bool sculpt_terrain(PrimitiveDesc& mesh, const TerrainGrid& grid, const TerrainBrush& brush) {
  const bool nothing_to_sculpt = !grid.fits(mesh) || brush.radius <= 0.0f;
  if (nothing_to_sculpt) return false;
  // Smoothing reads the heights as they were before the stroke.
  const PrimitiveDesc before = brush.mode == SculptMode::Smooth ? mesh : PrimitiveDesc{};
  const Stroke stroke{brush, terrain_height(mesh, grid, brush.x, brush.z), before};
  const bool touched = sculpt_reached(mesh, grid, stroke);
  if (touched) update_terrain_normals(mesh, grid);
  return touched;
}

}  // namespace redgem
