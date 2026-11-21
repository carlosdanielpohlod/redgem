#pragma once

// What the ground of a terrain's mesh is like under each texel of its splat map: how high, how
// steep. Rules paint layers by it (see ground_rules.hpp).

#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {

// Where a splat map lies: over the mesh's own X and Z bounds, at least a meter along each (a
// mesh flat along one gets its map over a meter there). Shared with what draws the layers.
struct SplatArea {
  float min_x = 0.0f;
  float min_z = 0.0f;
  float size_x = 1.0f;
  float size_z = 1.0f;
};
SplatArea splat_area(const Bounds& bounds);

// Where a spot of the mesh (its own X and Z) is on the map, 0 to 1 across its columns and rows.
struct SplatSpot {
  float u = 0.0f;
  float v = 0.0f;
};
SplatSpot splat_spot(const SplatArea& area, float x, float z);

// The middle of texel (`column`, `row`) of a map of `size` texels a side, in the mesh's X and Z.
struct GroundPlace {
  float x = 0.0f;
  float z = 0.0f;
};
GroundPlace texel_place(const SplatArea& area, int size, int column, int row);

// The ground under a texel: the highest surface of the mesh there.
struct GroundPoint {
  bool covered = false;  // the mesh has ground over the texel's middle
  float height = 0.0f;   // meters, in the mesh's own space
  float slope = 0.0f;    // degrees from flat: 0 level, 90 a wall
};

// The ground under every texel of a map of `size` texels a side laid over the mesh, row by row.
std::vector<GroundPoint> survey_ground(const MeshDesc& mesh, int size);

}  // namespace redgem
