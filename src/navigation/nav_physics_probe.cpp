#include "navigation/nav_physics_probe.hpp"

#include <algorithm>

namespace redgem {
namespace {

// The least gap between the floor and the part of a character tested for room, so that the floor
// it stands on is never taken for something it is inside.
constexpr float kFloorGap = 0.05f;
// Half a cell's diagonal, per meter of its side: sqrt(2) / 2.
constexpr float kHalfDiagonal = 0.70710678f;

// The part of a character above the highest step it climbs, from `bottom` up: an upright capsule
// of `radius` (at least a ball, for a character shorter than it is wide). Its round bottom lets it
// stand on a slope without touching the ground beside its feet.
QueryShape character_capsule(const NavSettings& settings, float radius, float bottom) {
  QueryShape shape;
  shape.kind = QueryShapeKind::Capsule;
  shape.radius = radius;
  shape.height = std::max(settings.agent_height - bottom, shape.radius * kTwo);
  return shape;
}

}  // namespace

PhysicsNavProbe::PhysicsNavProbe(const PhysicsWorld& physics, const NavSettings& settings, float cell_size, float top,
                                 float bottom)
    : physics_(physics), top_(top), depth_(std::max(top - bottom, 0.0f)) {
  const float lowest = std::max(settings.max_step, kFloorGap);
  body_ = character_capsule(settings, std::max(settings.agent_radius, cell_size * kHalfDiagonal), lowest);
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

}  // namespace redgem
