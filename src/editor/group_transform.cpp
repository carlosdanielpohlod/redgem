#include "editor/group_transform.hpp"

#include <cmath>

namespace redgem::group_transform {
namespace {

constexpr int kMatrixSide = 4;  // a placement as a 4x4 matrix, column by column (as glTF keeps it)
constexpr int kXColumn = 0;
constexpr int kYColumn = 1;
constexpr int kZColumn = 2;
constexpr int kTranslationColumn = 3;
constexpr int kXRow = 0;  // where x, y and z are in a column
constexpr int kYRow = 1;
constexpr int kZRow = 2;
constexpr Vec3 kUnitX{1.0f, 0.0f, 0.0f};
constexpr Vec3 kUnitY{0.0f, 1.0f, 0.0f};
constexpr Vec3 kUnitZ{0.0f, 0.0f, 1.0f};
constexpr float kEvenScale = 1e-6f;  // how far apart a pivot's scales may be and still count as even

Vec3 middle_of(const std::vector<Transform>& worlds) {
  Vec3 sum = Vec3::zero();
  for (const Transform& world : worlds) sum = sum + world.position;
  return sum * (1.0f / static_cast<float>(worlds.size()));
}

// The pivot scales the same along each of its axes: a turned object stays exactly what it was,
// only bigger or smaller (and a mirrored one stays mirrored).
bool scales_evenly(const Transform& pivot) {
  return std::fabs(pivot.scale.x - pivot.scale.y) <= kEvenScale &&
         std::fabs(pivot.scale.y - pivot.scale.z) <= kEvenScale;
}

// One axis of an object seen from the pivot (`axis` of `local`, `length` long), as the pivot
// carries it now: stretched along the pivot's own axes, then turned.
Vec3 carried_axis(const Transform& local, Vec3 axis, float length, const Transform& pivot) {
  return rotate(pivot.rotation, hadamard(pivot.scale, rotate(local.rotation, axis) * length));
}

void put_column(float* matrix, int column, Vec3 value) {
  matrix[column * kMatrixSide + kXRow] = value.x;
  matrix[column * kMatrixSide + kYRow] = value.y;
  matrix[column * kMatrixSide + kZRow] = value.z;
}

// An object stretched along the pivot's axes, which are not its own when it is turned: its axes
// are carried one by one and the placement nearest to them taken (one without shear).
Transform stretched_by(const Transform& pivot, const Transform& local) {
  float matrix[kMatrixSide * kMatrixSide] = {};
  put_column(matrix, kXColumn, carried_axis(local, kUnitX, local.scale.x, pivot));
  put_column(matrix, kYColumn, carried_axis(local, kUnitY, local.scale.y, pivot));
  put_column(matrix, kZColumn, carried_axis(local, kUnitZ, local.scale.z, pivot));
  put_column(matrix, kTranslationColumn,
             pivot.position + rotate(pivot.rotation, hadamard(pivot.scale, local.position)));
  matrix[kMatrixSide * kMatrixSide - 1] = 1.0f;
  return transform_from_matrix(matrix);
}

}  // namespace

Transform pivot_of(const std::vector<Transform>& worlds, PivotMode mode) {
  Transform pivot;
  if (worlds.empty()) return pivot;
  pivot.rotation = worlds.front().rotation;
  pivot.position = mode == PivotMode::First ? worlds.front().position : middle_of(worlds);
  return pivot;
}

Transform follow_pivot(const Transform& start, const Transform& pivot_start, const Transform& pivot_now) {
  // Where the object is as seen from the pivot does not change; the pivot does. (The pivot starts
  // unscaled, so seeing the object from it loses nothing.)
  const Transform local = relative_to(pivot_start, start);
  if (scales_evenly(pivot_now)) return combine(pivot_now, local);
  return stretched_by(pivot_now, local);
}

Drag begin_drag(const std::vector<int>& objects, const std::vector<Transform>& starts, PivotMode mode) {
  const Transform pivot = pivot_of(starts, mode);
  return Drag{objects, starts, pivot, pivot};
}

std::vector<Transform> placements(const Drag& drag) {
  std::vector<Transform> out;
  out.reserve(drag.starts.size());
  for (const Transform& start : drag.starts) out.push_back(follow_pivot(start, drag.pivot_start, drag.pivot_now));
  return out;
}

}  // namespace redgem::group_transform
