#pragma once

// What the Scene view draws of an object's joint: its anchor, the axis a hinge turns around or a
// slider slides along, how far its limits let it go, and a line to what it is held to.

#include <optional>
#include <vector>

#include "scene/scene.hpp"

namespace redgem {

// Meters the gizmo's parts reach from the anchor.
inline constexpr float kJointAxisHalfLength = 0.75f;
inline constexpr float kJointArcRadius = 0.5f;
inline constexpr int kJointArcSegments = 24;

struct JointGizmo {
  Vec3 anchor;
  // Hinges and sliders: the axis line through the anchor, from `axis_start` to `axis_end`.
  std::optional<Vec3> axis_start;
  std::optional<Vec3> axis_end;
  // Limited hinges: an arc around the axis from the low limit to the high one; limited sliders:
  // the stretch of the axis between them. Empty when free.
  std::vector<Vec3> limits;
  // Where its other end is: the other object, or a distance joint's other anchor.
  std::optional<Vec3> other_end;
};

// The gizmo of the joint an object of `scene` has (its `joint` properties), where the object is
// now; none without a joint.
std::optional<JointGizmo> joint_gizmo(const Scene& scene, ObjectId id);

}  // namespace redgem
