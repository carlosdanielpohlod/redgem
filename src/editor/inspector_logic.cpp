#include "editor/inspector_logic.hpp"

#include <algorithm>
#include <set>

#include "scene/props.hpp"

namespace redgem::inspector_logic {
namespace {

constexpr double kDefaultNumber = 0.0;  // what a new Number property starts as
constexpr int kAxisX = 0;               // the numbers of a vector, in order
constexpr int kAxisY = 1;

// Properties edited by dedicated widgets instead of the generic list.
const std::set<std::string>& reserved_props() {
  static const std::set<std::string> keys = {
      props::kScript, props::kBody,    props::kCollider, props::kTrigger, props::kMass,        props::kTag,
      props::kWheel,  props::kVisible, props::kPrefab,   props::kTerrain, props::kTerrainSize, props::kTerrainCells};
  return keys;
}

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
  return body_config_from_props(props).wants_physics() || props.count(props::kCollider) != 0;
}

bool has_mass(const BodyConfig& body) { return body.type == BodyType::Dynamic || body.type == BodyType::Vehicle; }

bool is_custom(const std::string& key) { return reserved_props().count(key) == 0; }

bool can_add_property(const PropertyMap& props, const std::string& key) {
  return !key.empty() && is_custom(key) && props.count(key) == 0;
}

bool textured(const MaterialDesc& material) {
  return material.base_color_image != kNoIndex || material.normal_image != kNoIndex;
}

bool wheel_card_shown(const std::optional<std::string>& parent_body, bool is_wheel) {
  return is_wheel || parent_body == std::optional<std::string>(props::kBodyVehicle);
}

}  // namespace redgem::inspector_logic
