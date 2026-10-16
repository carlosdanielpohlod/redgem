#include "scene/shapes/terrain.hpp"

#include <algorithm>
#include <cmath>

#include "scene/scene_config.hpp"

namespace redgem {
namespace {

constexpr std::size_t kFloatsPerVertex = config::kFloatsPerPosition;
constexpr float kFullWeight = 1.0f;
constexpr float kTextureRepeat = 4.0f;     // meters covered by one repeat of the texture
constexpr float kSlopeSpan = 2.0f;         // a slope is measured across two cells
constexpr float kLayoutTolerance = 1e-3f;  // meters

void add_grid_vertices(PrimitiveDesc& mesh, const TerrainGrid& grid) {
  for (int row = 0; row < grid.side(); ++row) {
    for (int column = 0; column < grid.side(); ++column) {
      const float x = grid.coordinate(column);
      const float z = grid.coordinate(row);
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
  const float slope_x = (vertex_height(mesh, grid, column + 1, row) - vertex_height(mesh, grid, column - 1, row)) / run;
  const float slope_z = (vertex_height(mesh, grid, column, row + 1) - vertex_height(mesh, grid, column, row - 1)) / run;
  return normalize(Vec3{-slope_x, kFullWeight, -slope_z});
}

// Whether a mesh has a position and a normal for every vertex of the grid, and nothing more.
bool has_the_vertices_of(const PrimitiveDesc& mesh, const TerrainGrid& grid) {
  const std::size_t vertices = static_cast<std::size_t>(grid.side()) * static_cast<std::size_t>(grid.side());
  return mesh.positions.size() == vertices * kFloatsPerVertex && mesh.normals.size() == mesh.positions.size();
}

bool is_near(float value, float expected) { return std::fabs(value - expected) < kLayoutTolerance; }

// Whether the vertices are laid out as make_terrain lays them out (a mesh re-exported by another
// program may have the same count in another order): the first one is a corner, the next its neighbour.
bool starts_as_a_grid(const PrimitiveDesc& mesh, const TerrainGrid& grid) {
  const float corner = grid.coordinate(0);
  return is_near(mesh.positions[kXOffset], corner) && is_near(mesh.positions[kZOffset], corner) &&
         is_near(mesh.positions[kFloatsPerVertex], grid.coordinate(1));
}

// `from` and `to`, mixed by how far (0 to 1) from the first to the second.
float mixed(float from, float to, float share) { return from * (kFullWeight - share) + to * share; }

}  // namespace

bool TerrainGrid::fits(const PrimitiveDesc& mesh) const {
  const bool has_no_cells = cells <= 0 || size <= 0.0f;
  if (has_no_cells) return false;
  return has_the_vertices_of(mesh, *this) && starts_as_a_grid(mesh, *this);
}

bool TerrainGrid::covers(float x, float z) const {
  const float half = size * kHalf;
  return x >= -half && x <= half && z >= -half && z <= half;
}

std::size_t TerrainGrid::vertex_offset(int column, int row) const {
  return (static_cast<std::size_t>(row) * static_cast<std::size_t>(side()) + static_cast<std::size_t>(column)) *
         kFloatsPerVertex;
}

float vertex_height(const PrimitiveDesc& mesh, const TerrainGrid& grid, int column, int row) {
  column = std::clamp(column, 0, grid.cells);
  row = std::clamp(row, 0, grid.cells);
  return mesh.positions[grid.vertex_offset(column, row) + kYOffset];
}

PrimitiveDesc make_terrain(const TerrainGrid& grid) {
  PrimitiveDesc mesh;
  add_grid_vertices(mesh, grid);
  add_grid_triangles(mesh, grid);
  return mesh;
}

float terrain_height(const PrimitiveDesc& mesh, const TerrainGrid& grid, float x, float z) {
  const bool on_the_ground = grid.fits(mesh) && grid.covers(x, z);
  if (!on_the_ground) return 0.0f;
  const float half = grid.size * kHalf;
  const float column = (x + half) / grid.spacing();
  const float row = (z + half) / grid.spacing();
  const int column0 = std::min(static_cast<int>(column), grid.cells - 1);
  const int row0 = std::min(static_cast<int>(row), grid.cells - 1);
  const float across = column - static_cast<float>(column0);
  const float near_side =
      mixed(vertex_height(mesh, grid, column0, row0), vertex_height(mesh, grid, column0 + 1, row0), across);
  const float far_side =
      mixed(vertex_height(mesh, grid, column0, row0 + 1), vertex_height(mesh, grid, column0 + 1, row0 + 1), across);
  return mixed(near_side, far_side, row - static_cast<float>(row0));
}

void update_terrain_normals(PrimitiveDesc& mesh, const TerrainGrid& grid) {
  if (!grid.fits(mesh)) return;
  for (int row = 0; row < grid.side(); ++row) {
    for (int column = 0; column < grid.side(); ++column) {
      const Vec3 normal = normal_at(mesh, grid, column, row);
      float* stored = &mesh.normals[grid.vertex_offset(column, row)];
      stored[kXOffset] = normal.x;
      stored[kYOffset] = normal.y;
      stored[kZOffset] = normal.z;
    }
  }
}

Bounds mesh_bounds_of(const PrimitiveDesc& mesh) {
  Bounds bounds;
  for (std::size_t at = 0; at + kFloatsPerVertex <= mesh.positions.size(); at += kFloatsPerVertex) {
    bounds.include(vec3_from(&mesh.positions[at]));
  }
  return bounds;
}

}  // namespace redgem
