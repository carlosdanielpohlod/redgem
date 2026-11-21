#pragma once

// The geometry of a decal (a mark laid on a surface: a bullet hole, a scorch): the box it is
// projected through, and the parts of a surface's triangles that fall inside it, with where each
// corner is on the decal's picture. All in the world.

#include <array>
#include <vector>

#include "core/math.hpp"

namespace redgem {

// The box a decal is projected through: centered where it was placed, `half_size` to each side
// across its picture (along `right` and `up`), `half_depth` in front of and behind the surface
// (along `normal`, which points out of the surface).
struct DecalBox {
  Vec3 center;
  Vec3 right;
  Vec3 up;
  Vec3 normal;
  float half_size = 0.0f;
  float half_depth = 0.0f;
};

// The box of a decal placed at `point` on a surface facing `normal`, `size` meters across and
// `depth` meters deep, its picture turned `spin_radians` about the normal. Its picture is upright
// (its up as near the world's up as the surface lets it), unless the surface faces up or down: then
// its up is the world's forward.
DecalBox decal_box(Vec3 point, Vec3 normal, float size, float depth, float spin_radians);

// A corner of a decal's piece: where it is, and where on the decal's picture (0 to 1 across, 0 to
// 1 up).
struct DecalCorner {
  Vec3 position;
  float u = 0.0f;
  float v = 0.0f;
};

// Appends to `out` the part of a triangle (its corners in the world, in the order that faces out of
// its surface) that is inside the box, as triangles of three corners each: nothing when the
// triangle faces away from the decal or misses the box.
void clip_to_decal(const DecalBox& box, const std::array<Vec3, 3>& triangle, std::vector<DecalCorner>& out);

// The corners of the box, in the world.
std::array<Vec3, 8> box_corners(const DecalBox& box);

}  // namespace redgem
