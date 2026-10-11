#include "render/culling.hpp"

namespace redgem {
namespace {

constexpr int kMatrixSize = 4;
constexpr int kAxes = 3;
constexpr int kW = 3;  // the row the others are compared against (clip-space w)

}  // namespace

Frustum frustum_from_matrix(const float* m) {
  // Gribb-Hartmann: each clip plane is the w row plus or minus one of the x, y, z rows.
  const auto row = [m](int r, int column) { return m[column * kMatrixSize + r]; };
  Frustum frustum;
  for (int axis = 0; axis < kAxes; ++axis) {
    for (int side = 0; side < 2; ++side) {
      const float sign = side == 0 ? 1.0f : -1.0f;
      Frustum::Plane plane;
      plane.normal = {row(kW, 0) + sign * row(axis, 0), row(kW, 1) + sign * row(axis, 1), row(kW, 2) + sign * row(axis, 2)};
      plane.distance = row(kW, 3) + sign * row(axis, 3);
      const float scale = length(plane.normal);
      if (scale > 0.0f) {
        plane.normal = plane.normal * (1.0f / scale);
        plane.distance /= scale;
      }
      frustum.planes[static_cast<std::size_t>(axis * 2 + side)] = plane;
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
