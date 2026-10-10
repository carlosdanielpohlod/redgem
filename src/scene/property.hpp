#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>

#include "core/math.hpp"

namespace redgem {

// A Blender custom property value as exported into glTF extras.
using PropertyValue = std::variant<bool, double, std::string>;
using PropertyMap = std::map<std::string, PropertyValue>;

std::optional<std::string> get_string(const PropertyMap& props, const std::string& key);
std::optional<double> get_number(const PropertyMap& props, const std::string& key);
// Accepts real booleans and also 0/1 numbers, since Blender stores toggles either way.
std::optional<bool> get_bool(const PropertyMap& props, const std::string& key);

// "#rrggbb" (sRGB, as color pickers show it) -> linear RGB. nullopt when malformed.
std::optional<Vec3> parse_hex_color(const std::string& text);

}  // namespace redgem
