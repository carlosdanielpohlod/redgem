#include "scene/property.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace redgem {
namespace {

const PropertyValue* find(const PropertyMap& props, const std::string& key) {
  const auto it = props.find(key);
  return it == props.end() ? nullptr : &it->second;
}

}  // namespace

std::optional<std::string> get_string(const PropertyMap& props, const std::string& key) {
  const PropertyValue* value = find(props, key);
  if (value == nullptr) return std::nullopt;
  if (const auto* text = std::get_if<std::string>(value)) return *text;
  return std::nullopt;
}

std::optional<double> get_number(const PropertyMap& props, const std::string& key) {
  const PropertyValue* value = find(props, key);
  if (value == nullptr) return std::nullopt;
  if (const auto* number = std::get_if<double>(value)) return *number;
  return std::nullopt;
}

std::optional<bool> get_bool(const PropertyMap& props, const std::string& key) {
  const PropertyValue* value = find(props, key);
  if (value == nullptr) return std::nullopt;
  if (const auto* flag = std::get_if<bool>(value)) return *flag;
  if (const auto* number = std::get_if<double>(value)) return *number != 0.0;
  return std::nullopt;
}

namespace {

constexpr std::size_t kHexColorLength = 7;  // "#rrggbb"
constexpr int kHexBase = 16;
constexpr int kHexDigitsPerChannel = 2;
constexpr float kMaxChannel = 255.0f;
constexpr float kSrgbGamma = 2.2f;

}  // namespace

std::optional<Vec3> parse_hex_color(const std::string& text) {
  if (text.size() != kHexColorLength || text.front() != '#') return std::nullopt;
  float channels[3] = {};
  for (int i = 0; i < 3; ++i) {
    const std::string digits = text.substr(1 + static_cast<std::size_t>(i) * kHexDigitsPerChannel, kHexDigitsPerChannel);
    if (!std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isxdigit(c) != 0; })) {
      return std::nullopt;
    }
    const float srgb = static_cast<float>(std::stoi(digits, nullptr, kHexBase)) / kMaxChannel;
    channels[i] = std::pow(srgb, kSrgbGamma);
  }
  return Vec3{channels[0], channels[1], channels[2]};
}

}  // namespace redgem
