#include "scene/transform.hpp"

namespace redgem {
namespace {

constexpr int kColumnStride = 4;
constexpr int kTranslationColumn = 12;

}  // namespace

Transform transform_from_matrix(const float* m) {
  const Vec3 c0{m[0], m[1], m[2]};
  const Vec3 c1{m[kColumnStride], m[kColumnStride + 1], m[kColumnStride + 2]};
  const Vec3 c2{m[2 * kColumnStride], m[2 * kColumnStride + 1], m[2 * kColumnStride + 2]};
  Transform t;
  t.position = {m[kTranslationColumn], m[kTranslationColumn + 1], m[kTranslationColumn + 2]};
  t.scale = {length(c0), length(c1), length(c2)};
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
