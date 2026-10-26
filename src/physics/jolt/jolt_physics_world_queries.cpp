// Part of the Jolt world: what a ray through it hits, what a shape moving through it touches first,
// and what a shape put somewhere overlaps.

#include "physics/jolt/jolt_physics_world.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>
#include <set>
#include <tuple>
#include <utility>

#include "physics/physics_config.hpp"

namespace redgem::jolt {
namespace {

// A ray with no direction or no length: it cannot hit anything.
bool goes_nowhere(Vec3 direction, float max_distance) { return length(direction) <= kEpsilon || max_distance <= 0.0f; }

// The bodies of the objects a query was told to ignore.
class IgnoredBodies final : public JPH::BodyFilter {
 public:
  explicit IgnoredBodies(std::vector<JPH::BodyID> bodies) : bodies_(std::move(bodies)) {}
  bool ShouldCollide(const JPH::BodyID& body) const override {
    return std::find(bodies_.begin(), bodies_.end(), body) == bodies_.end();
  }

 private:
  std::vector<JPH::BodyID> bodies_;
};

// Nearest first; at the same distance by object, so the order never depends on Jolt's.
bool nearer(const RaycastHit& a, const RaycastHit& b) {
  return std::tie(a.distance, a.object) < std::tie(b.distance, b.object);
}

bool has_room(const std::vector<RaycastHit>& hits) { return hits.size() < config::kMaxQueryHits; }

// Each object once, at its nearest; nearest first; at most kMaxQueryHits of them.
std::vector<RaycastHit> nearest_of_each(std::vector<RaycastHit> hits) {
  std::sort(hits.begin(), hits.end(), nearer);
  std::vector<RaycastHit> kept;
  std::set<ObjectId> seen;
  for (const RaycastHit& hit : hits) {
    if (!has_room(kept)) break;
    if (seen.insert(hit.object).second) kept.push_back(hit);
  }
  return kept;
}

// The surface normal of a touched shape, out of it and toward the query (zero when Jolt gives no
// direction, as for shapes that only just touch at a point).
Vec3 contact_normal(JPH::Vec3Arg penetration_axis) {
  if (penetration_axis.IsNearZero()) return Vec3::zero();
  return from_jolt(-penetration_axis.Normalized());
}

JPH::RefConst<JPH::Shape> box_shape(Vec3 half_extents) {
  const float smallest = std::min({half_extents.x, half_extents.y, half_extents.z});
  const float convex_radius = std::min(JPH::cDefaultConvexRadius, smallest * config::kConvexRadiusFraction);
  return new JPH::BoxShape(to_jolt(half_extents), convex_radius);
}

// A capsule as tall as its two ends together is a sphere.
JPH::RefConst<JPH::Shape> capsule_shape(float radius, float height) {
  const float cylinder_half_height = height * kHalf - radius;
  if (cylinder_half_height <= kEpsilon) return new JPH::SphereShape(radius);
  return new JPH::CapsuleShape(cylinder_half_height, radius);
}

JPH::RefConst<JPH::Shape> shape_of(const QueryShape& shape) {
  switch (shape.kind) {
    case QueryShapeKind::Box:
      return box_shape(shape.half_extents);
    case QueryShapeKind::Capsule:
      return capsule_shape(shape.radius, shape.height);
    case QueryShapeKind::Sphere:
      break;
  }
  return new JPH::SphereShape(shape.radius);
}

}  // namespace

JPH::BodyID World::body_of(ObjectId object) const {
  const Record* record = find(object);
  if (record == nullptr) return JPH::BodyID();
  return is_character(record) ? record->character->GetInnerBodyID() : record->body;
}

std::vector<JPH::BodyID> World::bodies_of(const std::vector<ObjectId>& objects) const {
  std::vector<JPH::BodyID> bodies;
  for (const ObjectId object : objects) {
    const JPH::BodyID body = body_of(object);
    if (!body.IsInvalid()) bodies.push_back(body);
  }
  return bodies;
}

Vec3 World::surface_normal(const JPH::RayCastResult& result, JPH::RVec3Arg point) const {
  JPH::BodyLockRead lock(system_.GetBodyLockInterface(), result.mBodyID);
  if (!lock.Succeeded()) return {};
  return from_jolt(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
}

RaycastHit World::ray_hit(const JPH::RRayCast& ray, const JPH::RayCastResult& result, float max_distance) const {
  RaycastHit hit;
  hit.object = owner_of(result.mBodyID);
  hit.distance = result.mFraction * max_distance;
  const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
  hit.point = from_jolt(JPH::Vec3(point));
  hit.normal = surface_normal(result, point);
  return hit;
}

std::optional<RaycastHit> World::raycast(Vec3 origin, Vec3 direction, float max_distance,
                                         const QueryFilter& filter) const {
  const Vec3 dir = normalize(direction);
  if (goes_nowhere(dir, max_distance)) return std::nullopt;
  const JPH::RRayCast ray(JPH::RVec3(to_jolt(origin)), to_jolt(dir * max_distance));
  JPH::RayCastResult result;
  const LayersInMask layer_filter(filter.layers);
  const IgnoredBodies body_filter(bodies_of(filter.ignore));
  if (!system_.GetNarrowPhaseQuery().CastRay(ray, result, {}, layer_filter, body_filter)) return std::nullopt;
  return ray_hit(ray, result, max_distance);
}

std::vector<RaycastHit> World::raycast_all(Vec3 origin, Vec3 direction, float max_distance,
                                           const QueryFilter& filter) const {
  const Vec3 dir = normalize(direction);
  if (goes_nowhere(dir, max_distance)) return {};
  const JPH::RRayCast ray(JPH::RVec3(to_jolt(origin)), to_jolt(dir * max_distance));
  JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
  const LayersInMask layer_filter(filter.layers);
  const IgnoredBodies body_filter(bodies_of(filter.ignore));
  system_.GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings(), collector, {}, layer_filter, body_filter);
  std::vector<RaycastHit> hits;
  for (const JPH::RayCastResult& result : collector.mHits) hits.push_back(ray_hit(ray, result, max_distance));
  return nearest_of_each(std::move(hits));
}

std::optional<RaycastHit> World::shape_cast(const QueryShape& shape, Vec3 from, Vec3 to,
                                            const QueryFilter& filter) const {
  const JPH::RefConst<JPH::Shape> swept = shape_of(shape);
  const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
      swept.GetPtr(), JPH::Vec3::sReplicate(kUnitScale), matrix_of(from, shape.rotation), to_jolt(to - from));
  JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
  const LayersInMask layer_filter(filter.layers);
  const IgnoredBodies body_filter(bodies_of(filter.ignore));
  system_.GetNarrowPhaseQuery().CastShape(cast, JPH::ShapeCastSettings(), JPH::RVec3::sZero(), collector, {},
                                          layer_filter, body_filter);
  if (!collector.HadHit()) return std::nullopt;
  const JPH::ShapeCastResult& result = collector.mHit;
  return RaycastHit{owner_of(result.mBodyID2), from_jolt(result.mContactPointOn2),
                    contact_normal(result.mPenetrationAxis), result.mFraction * length(to - from)};
}

std::vector<RaycastHit> World::overlap(const QueryShape& shape, Vec3 center, const QueryFilter& filter) const {
  const JPH::RefConst<JPH::Shape> tested = shape_of(shape);
  JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
  const LayersInMask layer_filter(filter.layers);
  const IgnoredBodies body_filter(bodies_of(filter.ignore));
  system_.GetNarrowPhaseQuery().CollideShape(tested.GetPtr(), JPH::Vec3::sReplicate(kUnitScale),
                                             matrix_of(center, shape.rotation), JPH::CollideShapeSettings(),
                                             JPH::RVec3::sZero(), collector, {}, layer_filter, body_filter);
  std::vector<RaycastHit> hits;
  for (const JPH::CollideShapeResult& result : collector.mHits) {
    const Vec3 point = from_jolt(result.mContactPointOn2);
    hits.push_back({owner_of(result.mBodyID2), point, contact_normal(result.mPenetrationAxis), length(point - center)});
  }
  return nearest_of_each(std::move(hits));
}

}  // namespace redgem::jolt
