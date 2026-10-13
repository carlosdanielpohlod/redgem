#pragma once

// What the Inspector decides about an object's properties without drawing anything: which ones
// are stored, which ones have a card of their own, and which names a new one may take.

#include <optional>
#include <string>

#include "physics/body_config.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::inspector_logic {

// The kinds of property Add Component > Property offers, in the order it lists them.
enum class PropertyKind { Number, Text, Toggle };

// A flag property that is only stored when it differs from its default.
std::optional<PropertyValue> flag_or_nothing(bool stored, bool value);
// A text property that is not stored while it is empty.
std::optional<PropertyValue> text_or_nothing(const std::string& text);
// What a new property of each kind starts as.
PropertyValue initial_value(PropertyKind kind);

// Whether the object has a Physics card: a body, a trigger or a collider.
bool has_physics(const PropertyMap& props);
// Whether a body has a mass to set (the ones gravity moves).
bool has_mass(const BodyConfig& body);
// Whether a property is shown in the generic list (the others have widgets of their own).
bool is_custom(const std::string& key);
// A name the object can take as a new property: not empty, not taken, not one with a widget of its own.
bool can_add_property(const PropertyMap& props, const std::string& key);
// Whether a material draws with pictures (a color or a normal map).
bool textured(const MaterialDesc& material);
// Whether an object shows the Vehicle wheel card: it is a wheel, or a child of a vehicle.
bool wheel_card_shown(const std::optional<std::string>& parent_body, bool is_wheel);

}  // namespace redgem::inspector_logic
