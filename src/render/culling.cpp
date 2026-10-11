#include "render/culling.hpp"

namespace redgem {
namespace {

constexpr int kMatrixSize = 4;
constexpr int kAxes = 3;
constexpr int kSidesPerAxis = 2;  // each axis has a plane on its positive and on its negative side
constexpr int kW = 3;             // the row the others are compared against (clip-space w)
// The columns of the matrix a plane's four numbers come from.
constexpr int kX = 0;
constexpr int kY = 1;
constexpr int kZ = 2;
constexpr int kDistance = 3;

}  // namespace

Frustum frustum_from_matrix(const float* m) {
  // Gribb-Hartmann: each clip plane is the w row plus or minus one of the x, y, z rows.
  const auto row = [m](int r, int column) { return m[column * kMatrixSize + r]; };
  Frustum frustum;
  for (int axis = 0; axis < kAxes; ++axis) {
    for (int side = 0; side < kSidesPerAxis; ++side) {
      const float sign = side == 0 ? 1.0f : -1.0f;
      Frustum::Plane plane;
      plane.normal = {row(kW, kX) + sign * row(axis, kX), row(kW, kY) + sign * row(axis, kY),
                      row(kW, kZ) + sign * row(axis, kZ)};
      plane.distance = row(kW, kDistance) + sign * row(axis, kDistance);
      const float scale = length(plane.normal);
      if (scale > 0.0f) {
        plane.normal = plane.normal * (1.0f / scale);
        plane.distance /= scale;
      }
      frustum.planes[static_cast<std::size_t>(axis * kSidesPerAxis + side)] = plane;
    }
  }
  return frustum;
}

bool sphere_in_frustum(const Frustum& frustum, Vec3 center, float radius) {
  for (const Frustum::Plane& plane : frustum.planes) {
    if (dot(plane.normal, center) + plane.distance < -radius) return false;
  }
  return true;
}

bool too_small_to_see(Vec3 center, float radius, Vec3 viewer, float min_angle) {
  const float distance = length(center - viewer);
  return distance > radius && radius < distance * min_angle;
}

bool outside_distance_range(Vec3 center, Vec3 viewer, float from, float to) {
  const float distance = length(center - viewer);
  return distance < from || distance >= to;
}

}  // namespace redgem
