#include "scene/terrain.hpp"

#include <algorithm>
#include <cmath>

#include "core/config.hpp"

namespace redgem {
namespace {

constexpr std::size_t kFloatsPerVertex = 3;
constexpr float kFullWeight = 1.0f;
constexpr float kTextureRepeat = 4.0f;  // meters covered by one repeat of the texture
constexpr float kRayStepShare = 0.5f;   // of a cell, between samples along a ray
constexpr int kRayRefinements = 12;     // halvings that pin the hit down
constexpr float kNeighbourCount = 4.0f;
constexpr float kSlopeSpan = 2.0f;         // a slope is measured across two cells
constexpr float kLayoutTolerance = 1e-3f;  // meters

// Where a vertex's floats start in a mesh's positions and normals.
std::size_t vertex_offset(const TerrainGrid& grid, int column, int row) {
  return (static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.side()) + static_cast<std::size_t>(column)) *
         kFloatsPerVertex;
}

// Where a column is along X (or a row along Z), in the terrain's own space.
float grid_coordinate(const TerrainGrid& grid, int index) {
  return -grid.size * kHalf + static_cast<float>(index) * grid.spacing();
}

float& height_ref(PrimitiveDesc& mesh, const TerrainGrid& grid, int column, int row) {
  return mesh.positions[vertex_offset(grid, column, row) + kYOffset];
}

float height_of(const PrimitiveDesc& mesh, const TerrainGrid& grid, int column, int row) {
  column = std::clamp(column, 0, grid.cells);
  row = std::clamp(row, 0, grid.cells);
  return mesh.positions[vertex_offset(grid, column, row) + kYOffset];
}
// Brush strength at a distance from its center: 1 in the middle, easing to 0 at the radius.
float falloff(float distance, float radius) {
  if (distance >= radius) return 0.0f;
  return kHalf * (kFullWeight + std::cos(kPi * distance / radius));
}

void add_grid_vertices(PrimitiveDesc& mesh, const TerrainGrid& grid) {
  for (int row = 0; row < grid.side(); ++row) {
    for (int column = 0; column < grid.side(); ++column) {
      const float x = grid_coordinate(grid, column);
      const float z = grid_coordinate(grid, row);
      mesh.positions.insert(mesh.positions.end(), {x, 0.0f, z});
      mesh.normals.insert(mesh.normals.end(), {0.0f, 1.0f, 0.0f});
      mesh.texcoords.insert(mesh.texcoords.end(), {x / kTextureRepeat, z / kTextureRepeat});
    }
  }
}

void add_grid_triangles(PrimitiveDesc& mesh, const TerrainGrid& grid) {
  for (int row = 0; row < grid.cells; ++row) {
    for (int column = 0; column < grid.cells; ++column) {
      const auto corner = static_cast<std::uint32_t>(row * grid.side() + column);
      const auto next_row = corner + static_cast<std::uint32_t>(grid.side());
      // Two triangles per cell, counter-clockwise seen from above.
      mesh.indices.insert(mesh.indices.end(), {corner, next_row, corner + 1, corner + 1, next_row, next_row + 1});
    }
  }
}

// The ground's normal at a vertex, from its slope across and along (the neighbours on each side).
Vec3 normal_at(const PrimitiveDesc& mesh, const TerrainGrid& grid, int column, int row) {
  const float run = kSlopeSpan * grid.spacing();
  const float slope_x = (height_of(mesh, grid, column + 1, row) - height_of(mesh, grid, column - 1, row)) / run;
  const float slope_z = (height_of(mesh, grid, column, row + 1) - height_of(mesh, grid, column, row - 1)) / run;
  return normalize(Vec3{-slope_x, kFullWeight, -slope_z});
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
  return (height_of(mesh, grid, column - 1, row) + height_of(mesh, grid, column + 1, row) +
          height_of(mesh, grid, column, row - 1) + height_of(mesh, grid, column, row + 1)) /
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
  const float across = grid_coordinate(grid, column) - stroke.brush.x;
  const float along = grid_coordinate(grid, row) - stroke.brush.z;
  const float weight = falloff(std::hypot(across, along), stroke.brush.radius);
  if (weight <= 0.0f) return false;
  float& height = height_ref(mesh, grid, column, row);
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

// A ray over a terrain, in the terrain's own space.
struct TerrainRay {
  const PrimitiveDesc& mesh;
  const TerrainGrid& grid;
  Vec3 origin;
  Vec3 direction;

  Vec3 at(float distance) const { return origin + direction * distance; }
  float above_ground(float distance) const {
    const Vec3 point = at(distance);
    return point.y - terrain_height(mesh, grid, point.x, point.z);
  }
  bool over_terrain(float distance) const {
    const Vec3 point = at(distance);
    const float half = grid.size * kHalf;
    return point.x >= -half && point.x <= half && point.z >= -half && point.z <= half;
  }
  bool in_clear_air(float distance) const { return over_terrain(distance) && above_ground(distance) > 0.0f; }
  bool under_ground(float distance) const { return over_terrain(distance) && above_ground(distance) <= 0.0f; }
};

// The ray crossed the surface between two samples, above it at `low` and under it at `high`:
// closes in on the crossing.
Vec3 crossing(const TerrainRay& ray, float low, float high) {
  for (int i = 0; i < kRayRefinements; ++i) {
    const float middle = (low + high) * kHalf;
    (ray.above_ground(middle) > 0.0f ? low : high) = middle;
  }
  Vec3 hit = ray.at(high);
  hit.y = terrain_height(ray.mesh, ray.grid, hit.x, hit.z);
  return hit;
}

// The ray is sampled every `step`, from one step on: the first sample that is under the ground,
// if any is within `max_distance`.
std::optional<int> first_sample_under_ground(const TerrainRay& ray, float step, float max_distance) {
  for (int sample = 1; step * static_cast<float>(sample) <= max_distance; ++sample) {
    if (ray.under_ground(step * static_cast<float>(sample))) return sample;
  }
  return std::nullopt;
}

}  // namespace
bool TerrainGrid::fits(const PrimitiveDesc& mesh) const {
  if (cells <= 0 || size <= 0.0f) return false;
  const std::size_t vertices = static_cast<std::size_t>(side()) * static_cast<std::size_t>(side());
  if (mesh.positions.size() != vertices * kFloatsPerVertex || mesh.normals.size() != mesh.positions.size())
    return false;
  // The vertices must also be laid out as make_terrain lays them out (a mesh re-exported by another
  // program may have the same count in another order): the first one is a corner, the next its neighbour.
  const float corner = -size * kHalf;
  return std::fabs(mesh.positions[0] - corner) < kLayoutTolerance &&
         std::fabs(mesh.positions[kZOffset] - corner) < kLayoutTolerance &&
         std::fabs(mesh.positions[kFloatsPerVertex] - (corner + spacing())) < kLayoutTolerance;
}

PrimitiveDesc make_terrain(const TerrainGrid& grid) {
  PrimitiveDesc mesh;
  add_grid_vertices(mesh, grid);
  add_grid_triangles(mesh, grid);
  return mesh;
}
float terrain_height(const PrimitiveDesc& mesh, const TerrainGrid& grid, float x, float z) {
  if (!grid.fits(mesh)) return 0.0f;
  const float half = grid.size * kHalf;
  if (x < -half || x > half || z < -half || z > half) return 0.0f;
  const float column = (x + half) / grid.spacing();
  const float row = (z + half) / grid.spacing();
  const int column0 = std::min(static_cast<int>(column), grid.cells - 1);
  const int row0 = std::min(static_cast<int>(row), grid.cells - 1);
  const float across = column - static_cast<float>(column0);
  const float along = row - static_cast<float>(row0);
  const float near_side =
      height_of(mesh, grid, column0, row0) * (kFullWeight - across) + height_of(mesh, grid, column0 + 1, row0) * across;
  const float far_side = height_of(mesh, grid, column0, row0 + 1) * (kFullWeight - across) +
                         height_of(mesh, grid, column0 + 1, row0 + 1) * across;
  return near_side * (kFullWeight - along) + far_side * along;
}

void update_terrain_normals(PrimitiveDesc& mesh, const TerrainGrid& grid) {
  if (!grid.fits(mesh)) return;
  for (int row = 0; row < grid.side(); ++row) {
    for (int column = 0; column < grid.side(); ++column) {
      const Vec3 normal = normal_at(mesh, grid, column, row);
      float* stored = &mesh.normals[vertex_offset(grid, column, row)];
      stored[kXOffset] = normal.x;
      stored[kYOffset] = normal.y;
      stored[kZOffset] = normal.z;
    }
  }
}

bool sculpt_terrain(PrimitiveDesc& mesh, const TerrainGrid& grid, const TerrainBrush& brush) {
  if (!grid.fits(mesh) || brush.radius <= 0.0f) return false;
  // Smoothing reads the heights as they were before the stroke.
  const PrimitiveDesc before = brush.mode == SculptMode::Smooth ? mesh : PrimitiveDesc{};
  const Stroke stroke{brush, terrain_height(mesh, grid, brush.x, brush.z), before};
  const bool touched = sculpt_reached(mesh, grid, stroke);
  if (touched) update_terrain_normals(mesh, grid);
  return touched;
}

std::optional<Vec3> terrain_raycast(const PrimitiveDesc& mesh, const TerrainGrid& grid, Vec3 origin, Vec3 direction,
                                    float max_distance) {
  if (!grid.fits(mesh)) return std::nullopt;
  const TerrainRay ray{mesh, grid, origin, direction};
  const float step = grid.spacing() * kRayStepShare;
  const std::optional<int> under = first_sample_under_ground(ray, step, max_distance);
  if (!under) return std::nullopt;
  // The sample before must be in clear air, or the ray started under the ground or came in from
  // the side below it.
  const float above = step * static_cast<float>(*under - 1);
  if (!ray.in_clear_air(above)) return std::nullopt;
  return crossing(ray, above, step * static_cast<float>(*under));
}

Bounds mesh_bounds_of(const PrimitiveDesc& mesh) {
  Bounds bounds;
  for (std::size_t at = 0; at + kFloatsPerVertex <= mesh.positions.size(); at += kFloatsPerVertex) {
    bounds.include(vec3_from(&mesh.positions[at]));
  }
  return bounds;
}

}  // namespace redgem
