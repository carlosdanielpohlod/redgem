#include "app/sound_walls.hpp"

#include <algorithm>

namespace redgem {
namespace {

// Meters: a surface this near either end of the ray is one that end stands on or is in.
constexpr float kWallMargin = 0.05f;

bool past_the_margin(const RaycastHit& hit) { return hit.distance > kWallMargin; }

// Whether a ray from `from` toward `to` crosses a static surface away from where it starts. One
// the start stands on or is in (the ground under a listener, the machine a sound plays from) is
// crossed at once, so it does not count; further surfaces of the same body still do.
bool crosses_from(const PhysicsWorld& physics, Vec3 from, Vec3 to, float reach) {
  const Vec3 way = to - from;
  const auto hits = physics.static_surfaces(from, way / length(way), reach);
  return std::any_of(hits.begin(), hits.end(), past_the_margin);
}

}  // namespace

// A wall stands between them when the ray crosses one from each end: the body the sound is in,
// seen only from the listener's side, is not a wall.
bool static_body_between(const PhysicsWorld& physics, Vec3 from, Vec3 to) {
  const float reach = length(to - from) - kWallMargin;
  if (reach <= kWallMargin) return false;
  if (!crosses_from(physics, from, to, reach)) return false;
  return crosses_from(physics, to, from, reach);
}

}  // namespace redgem
