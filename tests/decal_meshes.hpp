#pragma once

// Meshes the decal tests lay marks on, made in code.

#include "scene/scene_desc.hpp"

namespace redgem::test {

// A square `side` meters across, flat on its own XZ plane, facing up (+Y), centered on its origin.
inline MeshDesc floor_quad(float side, int material = kNoIndex) {
  const float h = side * kHalf;
  PrimitiveDesc quad;
  quad.positions = {-h, 0, -h, h, 0, -h, h, 0, h, -h, 0, h};
  quad.normals = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
  quad.texcoords = {0, 0, 1, 0, 1, 1, 0, 1};
  quad.indices = {3, 2, 1, 3, 1, 0};  // counter-clockwise seen from above
  quad.material = material;
  MeshDesc mesh;
  mesh.name = "Floor";
  mesh.primitives = {quad};
  mesh.bounds.include({-h, 0, -h});
  mesh.bounds.include({h, 0, h});
  return mesh;
}

}  // namespace redgem::test
