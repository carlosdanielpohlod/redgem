#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "physics/body_config.hpp"
#include "scene/scene.hpp"

namespace redgem {
// Debug tessellation is bounded independently of mesh size or the number of scene objects.
inline constexpr std::size_t kMaxPhysicsDebugTriangles = 8192;
inline constexpr std::size_t kMaxDebugTrianglesPerCollider = 256;
inline constexpr std::size_t kDebugTriangleCorners = 3;
struct PhysicsDebugTriangle {
  ObjectId object = kNoObject;
  BodyType type = BodyType::None;
  bool trigger = false;
  std::array<Vec3, kDebugTriangleCorners> corners{};
};
struct PhysicsDebugGeometry {
  std::vector<PhysicsDebugTriangle> triangles;
  bool truncated = false;
};
}  // namespace redgem
