#include "editor/inspector_logic.hpp"

#include <algorithm>
#include <iterator>
#include <set>

#include "scene/props.hpp"

namespace redgem::inspector_logic {
namespace {

constexpr double kDefaultNumber = 0.0;  // what a new Number property starts as
constexpr int kAxisX = 0;               // the numbers of a vector, in order
constexpr int kAxisY = 1;

// Properties edited by dedicated widgets instead of the generic list.
const std::set<std::string>& reserved_props() {
  static const std::set<std::string> keys = {props::kScript, props::kBody,    props::kCollider,    props::kTrigger,
                                             props::kMass,   props::kTag,     props::kWheel,       props::kVisible,
                                             props::kPrefab, props::kTerrain, props::kTerrainSize, props::kTerrainCells,
                                             props::kLayer};
  static const std::set<std::string> with_joints = [] {
    std::set<std::string> all = keys;
    for (const std::string& key : joint_property_keys()) all.insert(key);
    return all;
  }();
  return with_joints;
}

constexpr float kDefaultRopeLength = 2.0f;   // meters above the object a new distance joint hangs it from
constexpr float kDefaultHingeLimit = 90.0f;  // degrees
constexpr float kDefaultSlideLimit = 1.0f;   // meters

}  // namespace

Vec3 part_of(const Transform& local, TransformPart part) {
  if (part == TransformPart::Position) return local.position;
  return part == TransformPart::Rotation ? quat_to_euler_degrees(local.rotation) : local.scale;
}

float axis_of(Vec3 value, int axis) {
  if (axis == kAxisX) return value.x;
  return axis == kAxisY ? value.y : value.z;
}

Vec3 with_axis(Vec3 value, int axis, float number) {
  (axis == kAxisX ? value.x : (axis == kAxisY ? value.y : value.z)) = number;
  return value;
}

Transform with_axis(Transform local, TransformPart part, int axis, float value, float min_scale) {
  const Vec3 changed = with_axis(part_of(local, part), axis, value);
  if (part == TransformPart::Position) local.position = changed;
  if (part == TransformPart::Rotation) local.rotation = quat_from_euler_degrees(changed);
  if (part == TransformPart::Scale) local.scale = with_axis(local.scale, axis, std::max(value, min_scale));
  return local;
}

std::optional<PropertyValue> flag_or_nothing(bool stored, bool value) {
  return stored ? std::optional<PropertyValue>(value) : std::nullopt;
}

std::optional<PropertyValue> text_or_nothing(const std::string& text) {
  return text.empty() ? std::nullopt : std::optional<PropertyValue>(text);
}

PropertyValue initial_value(PropertyKind kind) {
  if (kind == PropertyKind::Number) return PropertyValue(kDefaultNumber);
  return kind == PropertyKind::Text ? PropertyValue(std::string()) : PropertyValue(false);
}

bool has_physics(const PropertyMap& props) {
  const bool has_collider = props.count(props::kCollider) != 0;
  const bool has_layer = props.count(props::kLayer) != 0;
  const bool has_settings = has_collider || has_layer;
  return has_settings || body_config_from_props(props).wants_physics();
}

std::optional<PropertyValue> collision_layer_value(const std::string& name) {
  if (name == kDefaultLayerName) return std::nullopt;
  return PropertyValue(name);
}

bool collision_layer_changes(const PropertyMap& props, const std::string& name) {
  const auto current = props.find(props::kLayer);
  const auto value = collision_layer_value(name);
  if (current == props.end()) return value.has_value();
  return !value || current->second != *value;
}

bool has_mass(const BodyConfig& body) { return body.type == BodyType::Dynamic || body.type == BodyType::Vehicle; }

bool is_custom(const std::string& key) { return reserved_props().count(key) == 0; }

bool can_add_property(const PropertyMap& props, const std::string& key) {
  return !key.empty() && is_custom(key) && props.count(key) == 0;
}

bool wheel_card_shown(const std::optional<std::string>& parent_body, bool is_wheel) {
  return is_wheel || parent_body == std::optional<std::string>(props::kBodyVehicle);
}

bool has_joint(const PropertyMap& props) { return props.count(props::kJoint) != 0; }

PropertyMap new_joint_props(JointKind kind) {
  PropertyMap joint{{props::kJoint, std::string(joint_kind_name(kind))}};
  if (joint_is_distance(kind)) joint[props::kJointOtherAnchor[kAxisY]] = static_cast<double>(kDefaultRopeLength);
  return joint;
}

bool kept_across_joint_kinds(const std::string& key) {
  const bool anchor =
      std::find(std::begin(props::kJointAnchor), std::end(props::kJointAnchor), key) != std::end(props::kJointAnchor);
  return anchor || key == props::kJointTo || key == props::kJointBreakForce;
}

std::pair<float, float> default_joint_limits(JointKind kind) {
  const float reach = kind == JointKind::Hinge ? kDefaultHingeLimit : kDefaultSlideLimit;
  return {-reach, reach};
}

}  // namespace redgem::inspector_logic
