// Part of the Jolt world: what a ray through it hits.

#include "physics/jolt/jolt_physics_world.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

namespace redgem::jolt {
namespace {

// A ray with no direction or no length: it cannot hit anything.
bool goes_nowhere(Vec3 direction, float max_distance) { return length(direction) <= kEpsilon || max_distance <= 0.0f; }

}  // namespace

JPH::BodyID World::body_of(ObjectId object) const {
  const Record* record = find(object);
  if (record == nullptr) return JPH::BodyID();
  return is_character(record) ? record->character->GetInnerBodyID() : record->body;
}

Vec3 World::surface_normal(const JPH::RayCastResult& result, JPH::RVec3Arg point) const {
  JPH::BodyLockRead lock(system_.GetBodyLockInterface(), result.mBodyID);
  if (!lock.Succeeded()) return {};
  return from_jolt(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
}

std::optional<RaycastHit> World::raycast(Vec3 origin, Vec3 direction, float max_distance, ObjectId ignore) const {
  const Vec3 dir = normalize(direction);
  if (goes_nowhere(dir, max_distance)) return std::nullopt;
  const JPH::RRayCast ray(JPH::RVec3(to_jolt(origin)), to_jolt(dir * max_distance));
  JPH::RayCastResult result;
  const JPH::IgnoreSingleBodyFilter body_filter(body_of(ignore));
  if (!system_.GetNarrowPhaseQuery().CastRay(ray, result, {}, {}, body_filter)) return std::nullopt;
  RaycastHit hit;
  hit.object = owner_of(result.mBodyID);
  hit.distance = result.mFraction * max_distance;
  const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
  hit.point = from_jolt(JPH::Vec3(point));
  hit.normal = surface_normal(result, point);
  return hit;
}

}  // namespace redgem::jolt
