#pragma once

#include <array>

#include "core/math.hpp"

namespace redgem {

// The six planes of a view volume, each as (normal, distance): a point p is inside when
// dot(normal, p) + distance >= 0 for all of them.
struct Frustum {
  struct Plane {
    Vec3 normal;
    float distance = 0.0f;
  };
  std::array<Plane, 6> planes;
};

// From a view-projection matrix in column-major order (OpenGL / raylib's MatrixToFloatV).
Frustum frustum_from_matrix(const float* column_major);

// False only when the sphere is certainly outside the view volume.
bool sphere_in_frustum(const Frustum& frustum, Vec3 center, float radius);

// True when a sphere looks smaller than `min_angle` (radians, radius over distance) from the
// viewer: small parts of far objects are not worth drawing.
bool too_small_to_see(Vec3 center, float radius, Vec3 viewer, float min_angle);

}  // namespace redgem
