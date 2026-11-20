#include "app/sound_walls.hpp"

#include <algorithm>
#include <vector>

namespace redgem {
namespace {

// Meters: a surface this near either end of the ray is one that end stands on or is in.
constexpr float kWallMargin = 0.05f;

std::vector<RaycastHit> surfaces_from(const PhysicsWorld& physics, Vec3 from, Vec3 to, float reach) {
  const Vec3 way = to - from;
  return physics.static_surfaces(from, way / length(way), reach);
}

// The bodies a ray starts on or in: the ground under its end, the machine a sound plays from.
std::vector<ObjectId> started_in(const std::vector<RaycastHit>& hits) {
  std::vector<ObjectId> bodies;
  for (const RaycastHit& hit : hits)
    if (hit.distance <= kWallMargin) bodies.push_back(hit.object);
  return bodies;
}

bool of_any(ObjectId object, const std::vector<ObjectId>& bodies) {
  return std::find(bodies.begin(), bodies.end(), object) != bodies.end();
}

// Whether a ray crosses a surface away from its start, of a body the other end is not on or in.
bool crosses_a_wall(const std::vector<RaycastHit>& hits, const std::vector<ObjectId>& other_end) {
  return std::any_of(hits.begin(), hits.end(), [&](const RaycastHit& hit) {
    return hit.distance > kWallMargin && !of_any(hit.object, other_end);
  });
}

}  // namespace

// The ray goes both ways, since a one-sided surface (a quad, a terrain from below) is crossed
// one way only. A body one end is on or in is no wall to the ray from the other end.
bool static_body_between(const PhysicsWorld& physics, Vec3 from, Vec3 to) {
  const float reach = length(to - from) - kWallMargin;
  if (reach <= kWallMargin) return false;
  const auto out = surfaces_from(physics, from, to, reach);
  const auto back = surfaces_from(physics, to, from, reach);
  return crosses_a_wall(out, started_in(back)) || crosses_a_wall(back, started_in(out));
}

}  // namespace redgem
