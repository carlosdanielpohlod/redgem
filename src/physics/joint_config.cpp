#include "physics/joint_config.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr int kVectorParts = 3;

// A number of the properties, when it is there and finite.
std::optional<float> number_of(const PropertyMap& props, const char* key) {
  const std::optional<double> value = get_number(props, key);
  if (!value || !std::isfinite(*value)) return std::nullopt;
  return static_cast<float>(*value);
}

Vec3 vector_of(const PropertyMap& props, const char* const (&keys)[kVectorParts], Vec3 fallback) {
  return {number_of(props, keys[kXOffset]).value_or(fallback.x), number_of(props, keys[kYOffset]).value_or(fallback.y),
          number_of(props, keys[kZOffset]).value_or(fallback.z)};
}

// A number that is never below zero.
float not_negative(const PropertyMap& props, const char* key, JointConfig& config) {
  const float value = number_of(props, key).value_or(0.0f);
  if (value >= 0.0f) return value;
  config.warnings.push_back(std::string(key) + " cannot be below 0; 0 is used");
  return 0.0f;
}

void read_kind(const std::string& name, JointConfig& config) {
  const std::optional<JointKind> kind = joint_kind_named(name);
  config.known_kind = kind.has_value();
  if (kind) {
    config.kind = *kind;
  } else {
    config.warnings.push_back("unknown joint '" + name + "' (" + joint_kind_list() + ")");
  }
}

void read_axis(const PropertyMap& props, JointConfig& config) {
  config.axis = vector_of(props, props::kJointAxis, Vec3::up());
  if (length(config.axis) > kEpsilon) return;
  config.warnings.emplace_back("a joint's axis cannot be zero; up is used");
  config.axis = Vec3::up();
}

// How far a kind's limits may reach either way: degrees for hinges, meters for sliders.
float farthest_limit(JointKind kind) { return kind == JointKind::Hinge ? kDegreesPerHalfTurn : kLongestSlide; }

bool one_limit_only(const std::optional<float>& low, const std::optional<float>& high) {
  return low.has_value() != high.has_value();
}

// The limits within their range (low -farthest..0, high 0..farthest), with a warning when they
// were out of it.
std::pair<float, float> within_range(float low, float high, float farthest, JointConfig& config) {
  const std::pair<float, float> kept{std::clamp(low, -farthest, 0.0f), std::clamp(high, 0.0f, farthest)};
  if (kept != std::make_pair(low, high)) {
    const std::string reach = std::to_string(static_cast<int>(farthest));
    config.warnings.push_back("a joint's low limit goes from -" + reach + " to 0 and its high one from 0 to " + reach +
                              "; the nearest are used");
  }
  return kept;
}

void read_limits(const PropertyMap& props, JointConfig& config) {
  const std::optional<float> low = number_of(props, props::kJointLow);
  const std::optional<float> high = number_of(props, props::kJointHigh);
  if (one_limit_only(low, high)) config.warnings.emplace_back("a joint's limits need both a low and a high one");
  if (!low || !high) return;
  if (*low >= *high) {
    config.warnings.emplace_back("a joint's low limit must be below its high one; it is left free");
    return;
  }
  config.limits = within_range(*low, *high, farthest_limit(config.kind), config);
}

void read_motor(const PropertyMap& props, JointConfig& config) {
  config.motor_speed = number_of(props, props::kJointMotorSpeed).value_or(0.0f);
  config.motor_torque = not_negative(props, props::kJointMotorTorque, config);
}

// A distance joint's max: none (or 0, as the editor shows none) is as far apart as the ends start.
std::optional<float> automatic_or(std::optional<float> max) {
  if (max && *max <= 0.0f) return std::nullopt;
  return max;
}

void read_distance(const PropertyMap& props, JointConfig& config) {
  config.other_anchor = vector_of(props, props::kJointOtherAnchor, Vec3::zero());
  config.min_distance = not_negative(props, props::kJointMin, config);
  config.max_distance = automatic_or(number_of(props, props::kJointMax));
  config.spring_frequency = not_negative(props, props::kJointSpring, config);
  config.spring_damping = not_negative(props, props::kJointDamping, config);
  if (!config.max_distance || *config.max_distance >= config.min_distance) return;
  config.warnings.emplace_back("a distance joint's max cannot be below its min; the min is used");
  config.max_distance = config.min_distance;
}

void read_by_kind(const PropertyMap& props, JointConfig& config) {
  if (joint_has_axis(config.kind)) {
    read_axis(props, config);
    read_limits(props, config);
  }
  if (joint_has_motor(config.kind)) read_motor(props, config);
  if (joint_is_distance(config.kind)) read_distance(props, config);
}

// Limits as the physics world takes them: radians for hinges.
std::optional<std::pair<float, float>> world_limits(const JointConfig& config) {
  if (!config.limits || config.kind != JointKind::Hinge) return config.limits;
  return std::make_pair(degrees_to_radians(config.limits->first), degrees_to_radians(config.limits->second));
}

void describe_distance(const JointConfig& config, JointDesc& desc) {
  desc.min_distance = config.min_distance;
  desc.max_distance = std::max(config.max_distance.value_or(length(desc.anchor_b - desc.anchor)), desc.min_distance);
  desc.spring_frequency = config.spring_frequency;
  desc.spring_damping = config.spring_damping;
}

// Where a joint's second point is: on the other object, or for the world that many meters from
// the object (in the world's axes, so that a rope hangs from above whatever the object's turn).
Vec3 second_point(const JointConfig& config, const JointEnds& ends) {
  if (ends.b == kNoObject) return ends.a_world.position + config.other_anchor;
  return point_in(ends.b_world, config.other_anchor);
}

}  // namespace

std::optional<JointConfig> joint_config_from_props(const PropertyMap& props) {
  if (props.count(props::kJoint) == 0) return std::nullopt;
  JointConfig config;
  read_kind(get_string(props, props::kJoint).value_or(""), config);  // (one not a word names no kind)
  config.other = get_string(props, props::kJointTo).value_or("");
  config.anchor = vector_of(props, props::kJointAnchor, Vec3::zero());
  read_by_kind(props, config);
  config.break_force = not_negative(props, props::kJointBreakForce, config);
  return config;
}

JointDesc joint_desc(const JointConfig& config, const JointEnds& ends) {
  JointDesc desc;
  desc.kind = config.kind;
  desc.a = ends.a;
  desc.b = ends.b;
  desc.anchor = point_in(ends.a_world, config.anchor);
  desc.anchor_b = second_point(config, ends);
  desc.axis = normalize(rotate(ends.a_world.rotation, config.axis));
  desc.limits = world_limits(config);
  desc.motor_speed = degrees_to_radians(config.motor_speed);
  desc.motor_torque = config.motor_torque;
  if (joint_is_distance(config.kind)) describe_distance(config, desc);
  desc.break_force = config.break_force;
  return desc;
}

Vec3 point_in(const Transform& world, Vec3 local) {
  return world.position + rotate(world.rotation, hadamard(world.scale, local));
}

const char* joint_kind_name(JointKind kind) { return props::kJointKinds[static_cast<int>(kind)]; }

std::optional<JointKind> joint_kind_named(const std::string& name) {
  const auto* found = std::find(std::begin(props::kJointKinds), std::end(props::kJointKinds), name);
  if (found == std::end(props::kJointKinds)) return std::nullopt;
  return static_cast<JointKind>(std::distance(std::begin(props::kJointKinds), found));
}

std::string joint_kind_list() {
  std::string list;
  for (const char* kind : props::kJointKinds) list += (list.empty() ? "" : ", ") + std::string(kind);
  return list;
}

bool joint_has_axis(JointKind kind) { return kind == JointKind::Hinge || kind == JointKind::Slider; }
bool joint_has_motor(JointKind kind) { return kind == JointKind::Hinge; }
bool joint_is_distance(JointKind kind) { return kind == JointKind::Distance; }

std::vector<std::string> joint_property_keys() {
  std::vector<std::string> keys(std::begin(props::kJointScalars), std::end(props::kJointScalars));
  for (const char* const* vector : {props::kJointAnchor, props::kJointOtherAnchor, props::kJointAxis}) {
    keys.insert(keys.end(), vector, vector + kVectorParts);
  }
  return keys;
}

}  // namespace redgem
