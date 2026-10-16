#pragma once

#include <cstddef>
#include <optional>

#include "core/math.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// What reshapes and probes a terrain is in parts: terrain.cpp (the grid and its heights),
// terrain_sculpt.cpp (brush strokes) and terrain_raycast.cpp (where a ray meets the ground).
//
// A terrain is a square grid of `cells` x `cells` quads, `size` meters across, centered on its
// object's origin, whose vertices can be raised and lowered. It is an ordinary mesh: these
// functions build it and reshape it. Vertices are stored row by row (rows along Z, columns
// along X), (cells + 1) to a row.
struct TerrainGrid {
  float size = 0.0f;
  int cells = 0;

  int side() const { return cells + 1; }  // vertices per row
  float spacing() const { return size / static_cast<float>(cells); }
  bool fits(const PrimitiveDesc& mesh) const;  // is `mesh` a grid of this shape?
  // Where a column is along X (or a row along Z), in the terrain's own space.
  float coordinate(int index) const { return -size * kHalf + static_cast<float>(index) * spacing(); }
  // Whether a point of the terrain's own space is over the terrain, its edges included.
  bool covers(float x, float z) const;
  // Where a vertex's floats start in a mesh's positions and normals.
  std::size_t vertex_offset(int column, int row) const;
};

// Height of a vertex of the grid. A column or a row outside the grid counts as the nearest one
// in it, so that the vertices on the edges have neighbours on every side.
float vertex_height(const PrimitiveDesc& mesh, const TerrainGrid& grid, int column, int row);

// What a brush does to the ground under it.
enum class SculptMode { Raise, Lower, Smooth, Flatten };

// A brush stroke: where it is on the terrain (in the terrain's own space), how far it reaches,
// and how much it does (meters for Raise and Lower; a 0..1 share for Smooth and Flatten).
struct TerrainBrush {
  float x = 0.0f;
  float z = 0.0f;
  float radius = 0.0f;
  float amount = 0.0f;
  SculptMode mode = SculptMode::Raise;
};

// A flat terrain at height 0, facing up.
PrimitiveDesc make_terrain(const TerrainGrid& grid);

// Height of the ground at a point of the terrain's own space (0 outside it).
float terrain_height(const PrimitiveDesc& mesh, const TerrainGrid& grid, float x, float z);

// Applies one stroke: strongest at the brush's center, fading to nothing at its radius. Normals
// are brought up to date. Returns false when the brush touches no vertex.
bool sculpt_terrain(PrimitiveDesc& mesh, const TerrainGrid& grid, const TerrainBrush& brush);

// Recomputes every normal from the heights.
void update_terrain_normals(PrimitiveDesc& mesh, const TerrainGrid& grid);

// Where a ray (in the terrain's own space, direction normalized) first meets the ground.
std::optional<Vec3> terrain_raycast(const PrimitiveDesc& mesh, const TerrainGrid& grid, Vec3 origin, Vec3 direction,
                                    float max_distance);

// The box around a mesh's vertices.
Bounds mesh_bounds_of(const PrimitiveDesc& mesh);

}  // namespace redgem
