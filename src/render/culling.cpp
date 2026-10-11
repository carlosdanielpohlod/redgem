#include "render/culling.hpp"

namespace redgem {
namespace {

constexpr int kMatrixSize = 4;
constexpr int kAxes = 3;
constexpr float kSides[] = {1.0f, -1.0f};  // each axis has a plane on its positive and on its negative side
constexpr int kW = 3;                      // the row the others are compared against (clip-space w)
// The columns of the matrix a plane's four numbers come from.
constexpr int kX = 0;
constexpr int kY = 1;
constexpr int kZ = 2;
constexpr int kDistance = 3;

// Gribb-Hartmann: each clip plane is the w row plus or minus (`side`) one of the x, y, z rows.
Frustum::Plane clip_plane(const float* m, int axis, float side) {
  const auto row = [m](int r, int column) { return m[column * kMatrixSize + r]; };
  Frustum::Plane plane;
  plane.normal = {row(kW, kX) + side * row(axis, kX), row(kW, kY) + side * row(axis, kY),
                  row(kW, kZ) + side * row(axis, kZ)};
  plane.distance = row(kW, kDistance) + side * row(axis, kDistance);
  const float scale = length(plane.normal);
  if (scale > 0.0f) {
    plane.normal = plane.normal * (1.0f / scale);
    plane.distance /= scale;
  }
  return plane;
}

}  // namespace

Frustum frustum_from_matrix(const float* m) {
  Frustum frustum;
  std::size_t next = 0;
  for (int axis = 0; axis < kAxes; ++axis) {
    for (const float side : kSides) frustum.planes[next++] = clip_plane(m, axis, side);
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
