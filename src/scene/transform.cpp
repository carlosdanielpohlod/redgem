#include "scene/transform.hpp"

namespace redgem {
namespace {

// Where each column of a column-major 4x4 matrix starts.
constexpr int kXAxisColumn = 0;
constexpr int kYAxisColumn = 4;
constexpr int kZAxisColumn = 8;
constexpr int kTranslationColumn = 12;

// The sign of the x scale of a placement that mirrors and of one that does not.
constexpr float kMirroredSign = -1.0f;
constexpr float kUnmirroredSign = 1.0f;

// Whether three axes make a left-handed basis (a mirror): no rotation turns them into x, y and z.
bool left_handed(Vec3 x_axis, Vec3 y_axis, Vec3 z_axis) { return dot(cross(x_axis, y_axis), z_axis) < 0.0f; }

}  // namespace

Transform transform_from_matrix(const float* m) {
  Vec3 c0 = vec3_from(m + kXAxisColumn);
  const Vec3 c1 = vec3_from(m + kYAxisColumn);
  const Vec3 c2 = vec3_from(m + kZAxisColumn);
  // A mirror is taken as a negative x scale, so the rest of the axes is a rotation.
  const float x_sign = left_handed(c0, c1, c2) ? kMirroredSign : kUnmirroredSign;
  c0 = c0 * x_sign;
  Transform t;
  t.position = vec3_from(m + kTranslationColumn);
  t.scale = {length(c0) * x_sign, length(c1), length(c2)};
  t.rotation = quat_from_basis(normalize(c0), normalize(c1), normalize(c2));
  return t;
}

Transform combine(const Transform& parent, const Transform& child) {
  Transform world;
  world.position = parent.position + rotate(parent.rotation, hadamard(parent.scale, child.position));
  world.rotation = normalize(parent.rotation * child.rotation);
  world.scale = hadamard(parent.scale, child.scale);
  return world;
}

Transform relative_to(const Transform& parent, const Transform& world) {
  const Quat inverse_rotation = conjugate(parent.rotation);
  Transform local;
  local.position = safe_divide(rotate(inverse_rotation, world.position - parent.position), parent.scale);
  local.rotation = normalize(inverse_rotation * world.rotation);
  local.scale = safe_divide(world.scale, parent.scale);
  return local;
}

}  // namespace redgem
