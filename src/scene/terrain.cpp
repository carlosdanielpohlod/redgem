#include "scene/terrain.hpp"

#include <algorithm>
#include <cmath>

#include "core/config.hpp"

namespace redgem {
namespace {

constexpr std::size_t kFloatsPerVertex = 3;
constexpr std::size_t kHeightOffset = 1;       // y, in a vertex's xyz
constexpr std::size_t kFloatsPerTexcoord = 2;
constexpr float kFullWeight = 1.0f;
constexpr float kTextureRepeat = 4.0f;         // meters covered by one repeat of the texture
constexpr float kRayStepShare = 0.5f;          // of a cell, between samples along a ray
constexpr int kRayRefinements = 12;            // halvings that pin the hit down
constexpr float kNeighbourCount = 4.0f;
constexpr float kSlopeSpan = 2.0f;             // a slope is measured across two cells
constexpr float kLayoutTolerance = 1e-3f;      // meters

float& height_ref(PrimitiveDesc& mesh, const TerrainGrid& grid, int column, int row) {
  return mesh.positions[(static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.side()) +
                         static_cast<std::size_t>(column)) * kFloatsPerVertex + kHeightOffset];
}

float height_of(const PrimitiveDesc& mesh, const TerrainGrid& grid, int column, int row) {
  column = std::clamp(column, 0, grid.cells);
  row = std::clamp(row, 0, grid.cells);
  return mesh.positions[(static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.side()) +
                         static_cast<std::size_t>(column)) * kFloatsPerVertex + kHeightOffset];
}

// Brush strength at a distance from its center: 1 in the middle, easing to 0 at the radius.
float falloff(float distance, float radius) {
  if (distance >= radius) return 0.0f;
  return kHalf * (kFullWeight + std::cos(kPi * distance / radius));
}

}  // namespace

bool TerrainGrid::fits(const PrimitiveDesc& mesh) const {
  if (cells <= 0 || size <= 0.0f) return false;
  const std::size_t vertices = static_cast<std::size_t>(side()) * static_cast<std::size_t>(side());
  if (mesh.positions.size() != vertices * kFloatsPerVertex || mesh.normals.size() != mesh.positions.size()) return false;
  // The vertices must also be laid out as make_terrain lays them out (a mesh re-exported by another
  // program may have the same count in another order): the first one is a corner, the next its neighbour.
  const float corner = -size * kHalf;
  return std::fabs(mesh.positions[0] - corner) < kLayoutTolerance &&
         std::fabs(mesh.positions[2] - corner) < kLayoutTolerance &&
         std::fabs(mesh.positions[kFloatsPerVertex] - (corner + spacing())) < kLayoutTolerance;
}

PrimitiveDesc make_terrain(const TerrainGrid& grid) {
  PrimitiveDesc mesh;
  const int side = grid.side();
  const float half = grid.size * kHalf;
  for (int row = 0; row < side; ++row) {
    for (int column = 0; column < side; ++column) {
      const float x = -half + static_cast<float>(column) * grid.spacing();
      const float z = -half + static_cast<float>(row) * grid.spacing();
      mesh.positions.insert(mesh.positions.end(), {x, 0.0f, z});
      mesh.normals.insert(mesh.normals.end(), {0.0f, 1.0f, 0.0f});
      mesh.texcoords.insert(mesh.texcoords.end(), {x / kTextureRepeat, z / kTextureRepeat});
    }
  }
  for (int row = 0; row < grid.cells; ++row) {
    for (int column = 0; column < grid.cells; ++column) {
      const auto corner = static_cast<std::uint32_t>(row * side + column);
      const auto next_row = corner + static_cast<std::uint32_t>(side);
      // Two triangles per cell, counter-clockwise seen from above.
      mesh.indices.insert(mesh.indices.end(), {corner, next_row, corner + 1, corner + 1, next_row, next_row + 1});
    }
  }
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
  const float near_side = height_of(mesh, grid, column0, row0) * (kFullWeight - across) +
                          height_of(mesh, grid, column0 + 1, row0) * across;
  const float far_side = height_of(mesh, grid, column0, row0 + 1) * (kFullWeight - across) +
                         height_of(mesh, grid, column0 + 1, row0 + 1) * across;
  return near_side * (kFullWeight - along) + far_side * along;
}

void update_terrain_normals(PrimitiveDesc& mesh, const TerrainGrid& grid) {
  if (!grid.fits(mesh)) return;
  const int side = grid.side();
  for (int row = 0; row < side; ++row) {
    for (int column = 0; column < side; ++column) {
      // The ground's slope across and along, from the neighbours on each side.
      const float slope_x = (height_of(mesh, grid, column + 1, row) - height_of(mesh, grid, column - 1, row)) /
                            (kSlopeSpan * grid.spacing());
      const float slope_z = (height_of(mesh, grid, column, row + 1) - height_of(mesh, grid, column, row - 1)) /
                            (kSlopeSpan * grid.spacing());
      const Vec3 normal = normalize(Vec3{-slope_x, kFullWeight, -slope_z});
      const std::size_t at = (static_cast<std::size_t>(row) * static_cast<std::size_t>(side) +
                              static_cast<std::size_t>(column)) * kFloatsPerVertex;
      mesh.normals[at] = normal.x;
      mesh.normals[at + 1] = normal.y;
      mesh.normals[at + 2] = normal.z;
    }
  }
}

bool sculpt_terrain(PrimitiveDesc& mesh, const TerrainGrid& grid, const TerrainBrush& brush) {
  if (!grid.fits(mesh) || brush.radius <= 0.0f) return false;
  const float half = grid.size * kHalf;
  const int first_column = std::max(static_cast<int>(std::floor((brush.x - brush.radius + half) / grid.spacing())), 0);
  const int last_column = std::min(static_cast<int>(std::ceil((brush.x + brush.radius + half) / grid.spacing())), grid.cells);
  const int first_row = std::max(static_cast<int>(std::floor((brush.z - brush.radius + half) / grid.spacing())), 0);
  const int last_row = std::min(static_cast<int>(std::ceil((brush.z + brush.radius + half) / grid.spacing())), grid.cells);
  const float center_height = terrain_height(mesh, grid, brush.x, brush.z);
  const PrimitiveDesc before = brush.mode == SculptMode::Smooth ? mesh : PrimitiveDesc{};  // smoothing reads old heights
  bool touched = false;
  for (int row = first_row; row <= last_row; ++row) {
    for (int column = first_column; column <= last_column; ++column) {
      const float x = -half + static_cast<float>(column) * grid.spacing();
      const float z = -half + static_cast<float>(row) * grid.spacing();
      const float weight = falloff(std::hypot(x - brush.x, z - brush.z), brush.radius);
      if (weight <= 0.0f) continue;
      touched = true;
      float& height = height_ref(mesh, grid, column, row);
      const float share = std::clamp(brush.amount * weight, 0.0f, kFullWeight);
      switch (brush.mode) {
        case SculptMode::Raise: height += brush.amount * weight; break;
        case SculptMode::Lower: height -= brush.amount * weight; break;
        case SculptMode::Flatten: height += (center_height - height) * share; break;
        case SculptMode::Smooth: {
          const float around = (height_of(before, grid, column - 1, row) + height_of(before, grid, column + 1, row) +
                                height_of(before, grid, column, row - 1) + height_of(before, grid, column, row + 1)) /
                               kNeighbourCount;
          height += (around - height) * share;
          break;
        }
      }
    }
  }
  if (touched) update_terrain_normals(mesh, grid);
  return touched;
}

std::optional<Vec3> terrain_raycast(const PrimitiveDesc& mesh, const TerrainGrid& grid, Vec3 origin, Vec3 direction,
                                    float max_distance) {
  if (!grid.fits(mesh)) return std::nullopt;
  const float half = grid.size * kHalf;
  const auto above = [&](float distance) {
    const Vec3 point = origin + direction * distance;
    return point.y - terrain_height(mesh, grid, point.x, point.z);
  };
  const auto inside = [&](float distance) {
    const Vec3 point = origin + direction * distance;
    return point.x >= -half && point.x <= half && point.z >= -half && point.z <= half;
  };
  const float step = grid.spacing() * kRayStepShare;
  float previous = 0.0f;
  bool previous_valid = inside(previous) && above(previous) > 0.0f;
  for (float distance = step; distance <= max_distance; distance += step) {
    if (!inside(distance)) {
      previous_valid = false;
      previous = distance;
      continue;
    }
    if (above(distance) <= 0.0f) {
      if (!previous_valid) return std::nullopt;  // it started under the ground, or came in from the side below it
      // The ray crossed the surface between the two samples: close in on the crossing.
      float low = previous;
      float high = distance;
      for (int i = 0; i < kRayRefinements; ++i) {
        const float middle = (low + high) * kHalf;
        (above(middle) > 0.0f ? low : high) = middle;
      }
      Vec3 hit = origin + direction * high;
      hit.y = terrain_height(mesh, grid, hit.x, hit.z);
      return hit;
    }
    previous_valid = true;
    previous = distance;
  }
  return std::nullopt;
}

Bounds mesh_bounds_of(const PrimitiveDesc& mesh) {
  Bounds bounds;
  for (std::size_t at = 0; at + kFloatsPerVertex <= mesh.positions.size(); at += kFloatsPerVertex) {
    bounds.include({mesh.positions[at], mesh.positions[at + 1], mesh.positions[at + 2]});
  }
  return bounds;
}

}  // namespace redgem
