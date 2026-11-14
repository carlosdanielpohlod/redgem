#pragma once

// What the Inspector decides about an object's properties without drawing anything: which ones
// are stored, which ones have a card of their own, and which names a new one may take.

#include <optional>
#include <string>
#include <vector>

#include "physics/body_config.hpp"
#include "physics/joint_config.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem::inspector_logic {

// The kinds of property Add Component > Property offers, in the order it lists them.
enum class PropertyKind { Number, Text, Toggle };

// A flag property that is only stored when it differs from its default.
std::optional<PropertyValue> flag_or_nothing(bool stored, bool value);
// A text property that is not stored while it is empty.
std::optional<PropertyValue> text_or_nothing(const std::string& text);
// What a new property of each kind starts as.
PropertyValue initial_value(PropertyKind kind);

// Whether the object has a Physics card, including authored layer-only settings.
bool has_physics(const PropertyMap& props);
std::optional<PropertyValue> collision_layer_value(const std::string& name);
bool collision_layer_changes(const PropertyMap& props, const std::string& name);
// Whether a body has a mass to set (the ones gravity moves).
bool has_mass(const BodyConfig& body);
// Whether a property is shown in the generic list (the others have widgets of their own).
bool is_custom(const std::string& key);
// A name the object can take as a new property: not empty, not taken, not one with a widget of its own.
bool can_add_property(const PropertyMap& props, const std::string& key);
// Whether an object shows the Vehicle wheel card: it is a wheel, or a child of a vehicle.
bool wheel_card_shown(const std::optional<std::string>& parent_body, bool is_wheel);

// --- Joints ------------------------------------------------------------------------------------

// Whether the object has a Joint card.
bool has_joint(const PropertyMap& props);
// What Add Component > Joint gives an object: a joint of that kind to the world, at the object's
// own origin (a distance joint hangs it from a point above it).
PropertyMap new_joint_props(JointKind kind);
// What a joint's limits start as once turned on: a quarter turn either way for a hinge, a meter
// either way for a slider.
std::pair<float, float> default_joint_limits(JointKind kind);

// --- Several objects at once ------------------------------------------------------------------

// The objects a field shown for several of them edits: the ones selected when typing in it began,
// so that typing committed after the selection changed goes where it was meant to. While no one
// types, it follows the selection.
class GroupTargets {
 public:
  const std::vector<int>& follow(bool typing, const std::vector<int>& selected) {
    if (!typing) targets_ = selected;
    return targets_;
  }

 private:
  std::vector<int> targets_;
};

// The value every one of `values` has; nullopt when they differ (a field then shows them as
// mixed) and when there is none.
template <typename Value>
std::optional<Value> shared_value(const std::vector<Value>& values) {
  if (values.empty()) return std::nullopt;
  for (const Value& value : values) {
    if (!(value == values.front())) return std::nullopt;
  }
  return values.front();
}

// The parts of a placement the Inspector edits, each as three numbers.
enum class TransformPart { Position, Rotation, Scale };
inline constexpr int kAxes = 3;  // x, y, z
// One part of a placement as the Inspector shows it (the rotation in degrees about each axis).
Vec3 part_of(const Transform& local, TransformPart part);
// The placement with one number of one part changed to `value` (a scale never below `min_scale`).
Transform with_axis(Transform local, TransformPart part, int axis, float value, float min_scale);
// One number of a vector: x, y or z.
float axis_of(Vec3 value, int axis);
Vec3 with_axis(Vec3 value, int axis, float number);

}  // namespace redgem::inspector_logic
