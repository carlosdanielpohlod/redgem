#pragma once

// A joint set up on a scene's object through its properties (scene/props.hpp, the editor's Joint
// card), and the joint it makes once the scene's bodies are in a physics world.

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "physics/physics_world.hpp"
#include "scene/property.hpp"
#include "scene/transform.hpp"

namespace redgem {

// Meters a slider's limits may reach either way (as Physics.joint takes them).
inline constexpr float kLongestSlide = 10000.0f;

struct JointConfig {
  JointKind kind = JointKind::Hinge;
  bool known_kind = true;  // false: the `joint` property names no kind, and nothing is made
  std::string other;       // the stable id of the other end; empty: the world
  Vec3 anchor = Vec3::zero();
  Vec3 axis = Vec3::up();
  Vec3 other_anchor = Vec3::zero();  // distance joints (see props::kJointOtherAnchor)
  // As the properties have them: degrees for hinges, meters for sliders. None: free.
  std::optional<std::pair<float, float>> limits;
  float motor_speed = 0.0f;  // degrees per second
  float motor_torque = 0.0f;
  float min_distance = 0.0f;
  std::optional<float> max_distance;  // none: as far apart as the ends start
  float spring_frequency = 0.0f;
  float spring_damping = 0.0f;
  float break_force = 0.0f;
  std::vector<std::string> warnings;  // what was wrong, and what was used instead
};

// The joint an object's properties ask for (see scene/props.hpp); none without a `joint` property.
// What is out of range is brought into it, with a warning.
std::optional<JointConfig> joint_config_from_props(const PropertyMap& props);

// The ends of a joint, as they are in the world when it is made (`b` kNoObject: the world, and
// `b_world` is not looked at).
struct JointEnds {
  ObjectId a = kNoObject;
  Transform a_world;
  ObjectId b = kNoObject;
  Transform b_world;
};

// What the physics world makes of a config between those ends: its points and axis in the world.
JointDesc joint_desc(const JointConfig& config, const JointEnds& ends);

// A point of an object's own space, in the world (its scale stretches it).
Vec3 point_in(const Transform& world, Vec3 local);

// The words for the kinds (scene/props.hpp kJointKinds).
const char* joint_kind_name(JointKind kind);
std::optional<JointKind> joint_kind_named(const std::string& name);
// Them all, as messages list them: "fixed, hinge, ...".
std::string joint_kind_list();

// What each kind takes besides its ends and anchor.
bool joint_has_axis(JointKind kind);    // hinges and sliders (with their limits)
bool joint_has_motor(JointKind kind);   // hinges
bool joint_is_distance(JointKind kind); // a rope or a spring

// Every property a joint is made of, as an object has them.
std::vector<std::string> joint_property_keys();

}  // namespace redgem
