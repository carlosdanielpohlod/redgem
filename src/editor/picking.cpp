#include "editor/picking.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace redgem {
namespace {

constexpr float kEmptyRadius = 0.5f;
constexpr int kAxes = 3;

float component(Vec3 v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }

}  // namespace

std::optional<float> intersect_ray_box(const PickRay& ray, const Transform& world, const Bounds& local) {
  if (!local.valid) return std::nullopt;
  // Move the ray into the box's unscaled local space, then scale it into mesh units.
  const Quat inverse = conjugate(world.rotation);
  const Vec3 origin = safe_divide(rotate(inverse, ray.origin - world.position), world.scale);
  const Vec3 direction = safe_divide(rotate(inverse, ray.direction), world.scale);
  float near_t = -std::numeric_limits<float>::max();
  float far_t = std::numeric_limits<float>::max();
  for (int axis = 0; axis < kAxes; ++axis) {
    const float o = component(origin, axis);
    const float d = component(direction, axis);
    const float lo = component(local.min, axis);
    const float hi = component(local.max, axis);
    if (std::fabs(d) < kEpsilon) {
      if (o < lo || o > hi) return std::nullopt;
      continue;
    }
    float t0 = (lo - o) / d;
    float t1 = (hi - o) / d;
    if (t0 > t1) std::swap(t0, t1);
    near_t = std::max(near_t, t0);
    far_t = std::min(far_t, t1);
    if (near_t > far_t) return std::nullopt;
  }
  if (far_t < 0.0f) return std::nullopt;
  // `t` is in local units of the scaled direction, which maps 1:1 to world distance along ray.direction.
  return std::max(near_t, 0.0f);
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
