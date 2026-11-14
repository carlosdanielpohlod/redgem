#include <doctest/doctest.h>

#include <algorithm>
#include <iterator>
#include <string>

#include "editor/inspector_logic.hpp"
#include "editor/joint_gizmo.hpp"
#include "physics/joint_config.hpp"
#include "scene/props.hpp"
#include "scene/scene_ids.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr float kQuarterTurn = 90.0f;

PropertyMap hinge_props() {
  return {{props::kJoint, std::string(props::kJointHinge)},
          {props::kJointAnchor[0], -0.5},
          {props::kJointLow, 0.0},
          {props::kJointHigh, static_cast<double>(kQuarterTurn)},
          {props::kJointMotorSpeed, 30.0},
          {props::kJointMotorTorque, 5.0}};
}

bool warned(const JointConfig& config, const std::string& text) {
  return std::any_of(config.warnings.begin(), config.warnings.end(),
                     [&text](const std::string& warning) { return warning.find(text) != std::string::npos; });
}

}  // namespace

TEST_CASE("an object without a joint property has no joint, and one with it has the joint it says") {
  CHECK_FALSE(joint_config_from_props({}).has_value());
  const auto hinge = joint_config_from_props(hinge_props());
  REQUIRE(hinge.has_value());
  CHECK(hinge->known_kind);
  CHECK(hinge->kind == JointKind::Hinge);
  CHECK(hinge->other.empty());
  CHECK(approx_equal(hinge->anchor, Vec3{-0.5f, 0.0f, 0.0f}, kTolerance));
  CHECK(approx_equal(hinge->axis, Vec3::up(), kTolerance));
  REQUIRE(hinge->limits.has_value());
  CHECK(hinge->limits->second == doctest::Approx(kQuarterTurn));
  CHECK(hinge->motor_torque == doctest::Approx(5.0f));
  CHECK(hinge->warnings.empty());
}

TEST_CASE("what is out of range in a joint's properties is brought back, with a warning") {
  PropertyMap props = hinge_props();
  props[props::kJointLow] = 10.0;  // above 0
  props[props::kJointHigh] = 400.0;  // past half a turn
  props[props::kJointAxis[1]] = 0.0;  // a zero axis
  props[props::kJointMotorTorque] = -1.0;
  const auto clamped = joint_config_from_props(props);
  REQUIRE(clamped.has_value());
  CHECK(clamped->limits->first == doctest::Approx(0.0f));
  CHECK(clamped->limits->second == doctest::Approx(kDegreesPerHalfTurn));
  CHECK(approx_equal(clamped->axis, Vec3::up(), kTolerance));
  CHECK(clamped->motor_torque == doctest::Approx(0.0f));
  CHECK(warned(*clamped, "axis"));
  CHECK(warned(*clamped, props::kJointMotorTorque));

  props.erase(props::kJointHigh);
  CHECK(warned(*joint_config_from_props(props), "both"));
  const auto unknown = joint_config_from_props({{props::kJoint, std::string("rope")}});
  CHECK_FALSE(unknown->known_kind);
  CHECK(warned(*unknown, "unknown joint 'rope'"));
}

TEST_CASE("a joint's points and axis follow the object into the world") {
  JointEnds ends;
  ends.a = 1;
  ends.a_world.position = {2, 0, 0};
  ends.a_world.scale = {2, 1, 1};
  ends.a_world.rotation = quat_from_axis_angle(Vec3::up(), degrees_to_radians(kQuarterTurn));
  PropertyMap props = hinge_props();
  props[props::kJointAxis[0]] = 1.0;
  props[props::kJointAxis[1]] = 0.0;
  const JointDesc desc = joint_desc(*joint_config_from_props(props), ends);
  CHECK(desc.kind == JointKind::Hinge);
  CHECK(desc.b == kNoObject);
  // Half its (doubled) width to its left, turned a quarter around up: one meter towards +z.
  CHECK(approx_equal(desc.anchor, Vec3{2, 0, 1}, kTolerance));
  CHECK(approx_equal(desc.axis, Vec3{0, 0, -1}, kTolerance));
  CHECK(desc.limits->second == doctest::Approx(degrees_to_radians(kQuarterTurn)));
  CHECK(desc.motor_speed == doctest::Approx(degrees_to_radians(30.0f)));
}

TEST_CASE("a distance joint is as long as its ends start apart unless it says") {
  JointEnds ends;
  ends.a = 1;
  ends.b = 2;
  ends.b_world.position = {0, 3, 0};
  PropertyMap props = {{props::kJoint, std::string(props::kJointDistance)}, {props::kJointTo, std::string("hook")}};
  JointDesc desc = joint_desc(*joint_config_from_props(props), ends);
  CHECK(approx_equal(desc.anchor_b, Vec3{0, 3, 0}, kTolerance));
  CHECK(desc.max_distance == doctest::Approx(3.0f));
  props[props::kJointMax] = 0.0;  // as the editor shows "as far as they start"
  CHECK(joint_desc(*joint_config_from_props(props), ends).max_distance == doctest::Approx(3.0f));
  props[props::kJointMax] = 1.5;
  props[props::kJointMin] = 2.0;
  const auto config = joint_config_from_props(props);
  CHECK(warned(*config, "max"));
  CHECK(joint_desc(*config, ends).max_distance == doctest::Approx(2.0f));
}

TEST_CASE("the kinds of joint are named as the scene's properties name them") {
  for (int i = 0; i < static_cast<int>(std::size(props::kJointKinds)); ++i) {
    const auto kind = static_cast<JointKind>(i);
    CHECK(joint_kind_named(joint_kind_name(kind)) == kind);
  }
  CHECK_FALSE(joint_kind_named("rope").has_value());
  CHECK(joint_has_axis(JointKind::Slider));
  CHECK_FALSE(joint_has_motor(JointKind::Slider));
}

TEST_CASE("a joint's properties have a card of their own, and Add Component > Joint starts one") {
  for (const std::string& key : joint_property_keys()) CHECK_FALSE(inspector_logic::is_custom(key));
  const PropertyMap hinge = inspector_logic::new_joint_props(JointKind::Hinge);
  CHECK(inspector_logic::has_joint(hinge));
  CHECK(get_string(hinge, props::kJoint) == std::optional<std::string>(props::kJointHinge));
  const PropertyMap rope = inspector_logic::new_joint_props(JointKind::Distance);
  CHECK(get_number(rope, props::kJointOtherAnchor[1]).value_or(0.0) > 0.0);  // hangs from above
  CHECK(inspector_logic::default_joint_limits(JointKind::Hinge).first < 0.0f);
}

TEST_CASE("a joint follows its other end to a copy, and keeps any other end") {
  PropertyMap props = {{props::kJointTo, std::string("frame")}};
  follow_new_identities(props, {{"door", "door2"}});
  CHECK(get_string(props, props::kJointTo) == std::optional<std::string>("frame"));
  follow_new_identities(props, {{"frame", "frame2"}});
  CHECK(get_string(props, props::kJointTo) == std::optional<std::string>("frame2"));
}

TEST_CASE("a limited hinge is drawn with its anchor, its axis and an arc from limit to limit") {
  SceneDesc desc;
  NodeDesc door;
  door.name = "Door";
  door.local.position = {1, 0, 0};
  door.props = hinge_props();
  desc.nodes.push_back(door);
  const Scene scene = Scene::from_desc(desc);
  const auto gizmo = joint_gizmo(scene, 0);
  REQUIRE(gizmo.has_value());
  CHECK(approx_equal(gizmo->anchor, Vec3{0.5f, 0, 0}, kTolerance));
  REQUIRE(gizmo->axis_end.has_value());
  CHECK(approx_equal(*gizmo->axis_end, Vec3{0.5f, kJointAxisHalfLength, 0}, kTolerance));
  REQUIRE(gizmo->limits.size() == static_cast<std::size_t>(kJointArcSegments + 1));
  // From towards the door (+x) at 0, a quarter turn around up to -z at the high limit.
  CHECK(approx_equal(gizmo->limits.front(), Vec3{0.5f + kJointArcRadius, 0, 0}, kTolerance));
  CHECK(approx_equal(gizmo->limits.back(), Vec3{0.5f, 0, -kJointArcRadius}, kTolerance));
  CHECK_FALSE(gizmo->other_end.has_value());
  desc.nodes.front().props.clear();
  CHECK_FALSE(joint_gizmo(Scene::from_desc(desc), 0).has_value());
}
