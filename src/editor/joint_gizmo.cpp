#include "editor/joint_gizmo.hpp"

#include <cmath>

#include "physics/joint_config.hpp"

namespace redgem {
namespace {

// A direction across `axis`: towards the object from its hinge, or any one when the object is on
// the axis.
Vec3 across(Vec3 axis, Vec3 toward) {
  const Vec3 flat = toward - axis * dot(toward, axis);
  if (length(flat) > kEpsilon) return normalize(flat);
  const Vec3 other = std::fabs(axis.y) < kHalf ? Vec3::up() : Vec3::right();
  return normalize(cross(axis, other));
}

std::vector<Vec3> hinge_arc(const JointDesc& desc, Vec3 center) {
  const Vec3 start = across(desc.axis, center - desc.anchor) * kJointArcRadius;
  const auto [low, high] = *desc.limits;
  std::vector<Vec3> arc;
  for (int step = 0; step <= kJointArcSegments; ++step) {
    const float angle = low + (high - low) * static_cast<float>(step) / static_cast<float>(kJointArcSegments);
    arc.push_back(desc.anchor + rotate(quat_from_axis_angle(desc.axis, angle), start));
  }
  return arc;
}

std::vector<Vec3> limit_marks(const JointDesc& desc, Vec3 center) {
  if (!desc.limits) return {};
  if (desc.kind == JointKind::Hinge) return hinge_arc(desc, center);
  return {desc.anchor + desc.axis * desc.limits->first, desc.anchor + desc.axis * desc.limits->second};
}

void add_axis(const JointDesc& desc, Vec3 center, JointGizmo& gizmo) {
  if (!joint_has_axis(desc.kind)) return;
  gizmo.axis_start = desc.anchor - desc.axis * kJointAxisHalfLength;
  gizmo.axis_end = desc.anchor + desc.axis * kJointAxisHalfLength;
  gizmo.limits = limit_marks(desc, center);
}

std::optional<Vec3> other_end_of(const JointDesc& desc, const Scene& scene) {
  if (joint_is_distance(desc.kind)) return desc.anchor_b;
  if (desc.b == kNoObject) return std::nullopt;
  return scene.world_transform(desc.b).position;
}

// The ends of the joint as the scene has them (an other end that is not there: the world).
JointEnds ends_in(const Scene& scene, ObjectId id, const JointConfig& config) {
  JointEnds ends{id, scene.world_transform(id), scene.find_by_stable_id(config.other), {}};
  if (ends.b != kNoObject) ends.b_world = scene.world_transform(ends.b);
  return ends;
}

}  // namespace

std::optional<JointGizmo> joint_gizmo(const Scene& scene, ObjectId id) {
  const std::optional<JointConfig> config = joint_config_from_props(scene.at(id).props);
  if (!config || !config->known_kind) return std::nullopt;
  const JointEnds ends = ends_in(scene, id, *config);
  const JointDesc desc = joint_desc(*config, ends);
  JointGizmo gizmo;
  gizmo.anchor = desc.anchor;
  add_axis(desc, ends.a_world.position, gizmo);
  gizmo.other_end = other_end_of(desc, scene);
  return gizmo;
}

}  // namespace redgem
