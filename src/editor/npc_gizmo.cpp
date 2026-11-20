#include "editor/npc_gizmo.hpp"

namespace redgem {
namespace {

constexpr float kBothSides = 2.0f;  // a cross reaches its size each way

// Where an object faces across the ground: its forward laid flat, or the world's forward when it
// looks straight up or down.
Vec3 facing(const Transform& world) {
  Vec3 forward = world.forward();
  forward.y = 0.0f;
  return length(forward) > kEpsilon ? normalize(forward) : Vec3::forward();
}

Vec3 turned(Vec3 direction, float degrees) {
  return rotate(quat_from_axis_angle(Vec3::up(), degrees_to_radians(degrees)), direction);
}

// Points across the ground around `center`, `radius` away, from `from` degrees off `direction` to `to`.
std::vector<Vec3> arc(Vec3 center, Vec3 direction, float radius, float from, float to, int segments) {
  std::vector<Vec3> points;
  for (int step = 0; step <= segments; ++step) {
    const float degrees = from + (to - from) * static_cast<float>(step) / static_cast<float>(segments);
    points.push_back(center + turned(direction, degrees) * radius);
  }
  return points;
}

void add_polyline(NpcGizmo& gizmo, const std::vector<Vec3>& points, NpcGizmoPart part) {
  for (std::size_t i = 1; i < points.size(); ++i) gizmo.lines.push_back({points[i - 1], points[i], part});
}

void add_ring(NpcGizmo& gizmo, Vec3 center, float radius, NpcGizmoPart part) {
  add_polyline(gizmo, arc(center, Vec3::forward(), radius, 0.0f, kNpcFullTurn, kNpcRingSegments), part);
}

bool sees_all_around(float fov) { return fov >= kNpcFullTurn; }

void add_interactable(const InteractableSettings& settings, const Transform& world, NpcGizmo& gizmo) {
  add_ring(gizmo, world.position, settings.range, NpcGizmoPart::Interactable);
  gizmo.labels.push_back({world.position + Vec3::up() * kNpcPromptLift, settings.prompt.value_or(kDefaultPromptKey)});
}

// The cone seen from its eyes, flat across the ground: its two edges and its far end.
void add_perception(const PerceptionSettings& settings, const Transform& world, NpcGizmo& gizmo) {
  add_ring(gizmo, world.position, settings.hearing, NpcGizmoPart::Hearing);
  const Vec3 eyes = world.position + Vec3::up() * settings.eyes;
  if (sees_all_around(settings.fov)) {
    add_ring(gizmo, eyes, settings.sight, NpcGizmoPart::Sight);
    return;
  }
  const float half = settings.fov * kHalf;
  const std::vector<Vec3> far_end = arc(eyes, facing(world), settings.sight, -half, half, kNpcArcSegments);
  add_polyline(gizmo, far_end, NpcGizmoPart::Sight);
  gizmo.lines.push_back({eyes, far_end.front(), NpcGizmoPart::Sight});
  gizmo.lines.push_back({eyes, far_end.back(), NpcGizmoPart::Sight});
}

void add_schedule(const std::vector<std::string>& plan, const Transform& world, NpcGizmo& gizmo) {
  std::string text;
  for (const std::string& entry : plan) text += (text.empty() ? "" : "\n") + entry;
  gizmo.labels.push_back({world.position + Vec3::up() * kNpcScheduleLift, text});
}

void add_cross(NpcGizmo& gizmo, Vec3 at) {
  for (const Vec3 axis : {Vec3::right(), Vec3::up(), Vec3::back()}) {
    const Vec3 reach = axis * (kNpcMarkSize / kBothSides);
    gizmo.lines.push_back({at - reach, at + reach, NpcGizmoPart::Route});
  }
}

// From where it is along the corners still ahead, and a cross where it goes.
void add_route(const NavRoute& route, const Transform& world, NpcGizmo& gizmo) {
  std::vector<Vec3> path{world.position};
  path.insert(path.end(), route.waypoints.begin(), route.waypoints.end());
  add_polyline(gizmo, path, NpcGizmoPart::Route);
  add_cross(gizmo, route.destination);
}

}  // namespace

NpcGizmo npc_gizmo(const NpcSettings& settings, const Transform& world) {
  NpcGizmo gizmo;
  if (settings.interactable) add_interactable(*settings.interactable, world, gizmo);
  if (settings.perception) add_perception(*settings.perception, world, gizmo);
  if (!settings.schedule.empty()) add_schedule(settings.schedule, world, gizmo);
  if (settings.route) add_route(*settings.route, world, gizmo);
  return gizmo;
}

}  // namespace redgem
