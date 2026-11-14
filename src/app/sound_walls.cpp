#include "app/sound_walls.hpp"

#include <algorithm>

namespace redgem {
namespace {

// Meters: a sound this near its listener, or this near the surface it sits on, is not behind it.
constexpr float kWallMargin = 0.05f;

}  // namespace

bool static_body_between(const PhysicsWorld& physics, Vec3 from, Vec3 to) {
  const Vec3 way = to - from;
  const float reach = length(way) - kWallMargin;
  if (reach <= kWallMargin) return false;
  const auto hits = physics.raycast_all(from, way / length(way), reach, QueryFilter{});
  return std::any_of(hits.begin(), hits.end(),
                     [&](const RaycastHit& hit) { return physics.body_type(hit.object) == BodyType::Static; });
}

}  // namespace redgem
