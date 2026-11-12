#include "navigation/nav_physics_probe.hpp"

#include <algorithm>
#include <array>

#include "physics/body_config.hpp"

namespace redgem {
namespace {

// The least gap between the floor and the part of a character tested for room, so that the floor
// it stands on is never taken for something it is inside.
constexpr float kFloorGap = 0.05f;
// The least radius tested for room (a character of radius 0 is a thin post).
constexpr float kThinnest = 0.01f;
constexpr std::size_t kBoxCorners = 8;
constexpr unsigned kCornerX = 1;
constexpr unsigned kCornerY = 2;
constexpr unsigned kCornerZ = 4;

// The part of a character above the highest step it climbs, from `bottom` up: an upright capsule
// of its radius (at least a ball, for a character shorter than it is wide). Its round bottom lets
// it stand on a slope without touching the ground beside its feet.
QueryShape character_capsule(const NavSettings& settings, float bottom) {
  QueryShape shape;
  shape.kind = QueryShapeKind::Capsule;
  shape.radius = std::max(settings.agent_radius, kThinnest);
  shape.height = std::max(settings.agent_height - bottom, shape.radius * kTwo);
  return shape;
}

bool is_static_body(const PhysicsWorld& physics, ObjectId id) {
  return physics.has_object(id) && physics.body_type(id) == BodyType::Static && !physics.is_trigger(id);
}

Vec3 corner(const Bounds& bounds, unsigned index) {
  return {(index & kCornerX) != 0 ? bounds.max.x : bounds.min.x, (index & kCornerY) != 0 ? bounds.max.y : bounds.min.y,
          (index & kCornerZ) != 0 ? bounds.max.z : bounds.min.z};
}

// Adds an object's box, turned and scaled as it is in the world.
void include_object(Bounds& level, const Bounds& local, const Transform& world) {
  for (unsigned index = 0; index < kBoxCorners; ++index) {
    level.include(world.position + rotate(world.rotation, hadamard(corner(local, index), world.scale)));
  }
}

Bounds local_bounds(const Scene& scene, const SceneDesc& desc, ObjectId id) {
  const GameObject& object = scene.at(id);
  return object.mesh != kNoIndex ? desc.meshes[object.mesh].bounds : default_collider_bounds();
}

}  // namespace

PhysicsNavProbe::PhysicsNavProbe(const PhysicsWorld& physics, const NavSettings& settings, float top, float bottom)
    : physics_(physics), top_(top), depth_(std::max(top - bottom, 0.0f)) {
  const float lowest = std::max(settings.max_step, kFloorGap);
  body_ = character_capsule(settings, lowest);
  body_lift_ = lowest + body_.height * kHalf;
}

std::vector<NavSurface> PhysicsNavProbe::surfaces(float x, float z) const {
  const std::vector<RaycastHit> hits = physics_.static_surfaces({x, top_, z}, -Vec3::up(), depth_);
  std::vector<NavSurface> found;
  found.reserve(hits.size());
  for (const RaycastHit& hit : hits) found.push_back({hit.point.y, hit.normal});
  return found;
}

bool PhysicsNavProbe::blocked(Vec3 feet) const {
  return physics_.overlaps_static(body_, feet + Vec3::up() * body_lift_);
}

Bounds static_level_bounds(const Scene& scene, const SceneDesc& desc, const PhysicsWorld& physics) {
  Bounds level;
  for (const ObjectId id : scene.alive_ids()) {
    if (is_static_body(physics, id)) include_object(level, local_bounds(scene, desc, id), scene.world_transform(id));
  }
  return level;
}

}  // namespace redgem
