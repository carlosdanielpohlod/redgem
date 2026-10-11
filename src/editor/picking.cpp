#include "editor/picking.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace redgem {
namespace {

constexpr float kEmptyRadius = 0.5f;
constexpr int kAxes = 3;

float component(Vec3 v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }

// The stretch of a ray that is inside a box, as distances along the ray.
struct Stretch {
  float near_t = -std::numeric_limits<float>::max();
  float far_t = std::numeric_limits<float>::max();
};

// Narrows `inside` to where the ray is between `lo` and `hi` on one axis. False when no part of it is.
bool clip_to_slab(float origin, float direction, float lo, float hi, Stretch& inside) {
  if (std::fabs(direction) < kEpsilon) return !(origin < lo) && !(origin > hi);  // along the slab: in or out for good
  float t0 = (lo - origin) / direction;
  float t1 = (hi - origin) / direction;
  if (t0 > t1) std::swap(t0, t1);
  inside.near_t = std::max(inside.near_t, t0);
  inside.far_t = std::min(inside.far_t, t1);
  return !(inside.near_t > inside.far_t);
}

}  // namespace

std::optional<float> intersect_ray_box(const PickRay& ray, const Transform& world, const Bounds& local) {
  if (!local.valid) return std::nullopt;
  // Move the ray into the box's unscaled local space, then scale it into mesh units.
  const Quat inverse = conjugate(world.rotation);
  const Vec3 origin = safe_divide(rotate(inverse, ray.origin - world.position), world.scale);
  const Vec3 direction = safe_divide(rotate(inverse, ray.direction), world.scale);
  Stretch inside;
  for (int axis = 0; axis < kAxes; ++axis) {
    const bool reaches = clip_to_slab(component(origin, axis), component(direction, axis), component(local.min, axis),
                                      component(local.max, axis), inside);
    if (!reaches) return std::nullopt;
  }
  if (inside.far_t < 0.0f) return std::nullopt;
  // `t` is in local units of the scaled direction, which maps 1:1 to world distance along ray.direction.
  return std::max(inside.near_t, 0.0f);
}

ObjectId pick_object(const Scene& scene, const SceneDesc& desc, const PickRay& ray) {
  ObjectId best = kNoObject;
  float best_distance = std::numeric_limits<float>::max();
  for (const ObjectId id : scene.alive_ids()) {
    const GameObject& object = scene.at(id);
    if (object.mesh == kNoIndex || !object.visible) continue;
    const auto hit = intersect_ray_box(ray, scene.world_transform(id), desc.meshes[object.mesh].bounds);
    if (hit && *hit < best_distance) {
      best_distance = *hit;
      best = id;
    }
  }
  return best;
}

void object_extent(const Scene& scene, const SceneDesc& desc, ObjectId id, Vec3& center, float& radius) {
  const Transform world = scene.world_transform(id);
  const GameObject& object = scene.at(id);
  if (object.mesh == kNoIndex) {
    center = world.position;
    radius = kEmptyRadius;
    return;
  }
  const Bounds& bounds = desc.meshes[object.mesh].bounds;
  center = world.position + rotate(world.rotation, hadamard(bounds.center(), world.scale));
  radius = length(hadamard(bounds.half_extents(), world.scale));
}

}  // namespace redgem
