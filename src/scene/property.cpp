#include "scene/property.hpp"

#include <algorithm>
#include <cctype>

#include "scene/color_space.hpp"

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
constexpr char kHexColorMark = '#';
constexpr std::size_t kFirstHexDigit = 1;  // after the mark
constexpr int kHexBase = 16;
constexpr std::size_t kHexDigitsPerChannel = 2;
// Which pair of digits holds each channel.
constexpr std::size_t kRedChannel = 0;
constexpr std::size_t kGreenChannel = 1;
constexpr std::size_t kBlueChannel = 2;

bool is_hex(const std::string& digits) {
  return std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

// One channel of "#rrggbb" as linear light; nullopt when its two characters are not hex digits.
std::optional<float> parse_hex_channel(const std::string& text, std::size_t channel) {
  const std::string digits = text.substr(kFirstHexDigit + channel * kHexDigitsPerChannel, kHexDigitsPerChannel);
  if (!is_hex(digits)) return std::nullopt;
  return linear_from_srgb(static_cast<float>(std::stoi(digits, nullptr, kHexBase)) / kMaxColorChannel);
}

bool is_written_as_a_hex_color(const std::string& text) {
  return text.size() == kHexColorLength && text.front() == kHexColorMark;
}

}  // namespace

std::optional<Vec3> parse_hex_color(const std::string& text) {
  if (!is_written_as_a_hex_color(text)) return std::nullopt;
  const std::optional<float> red = parse_hex_channel(text, kRedChannel);
  const std::optional<float> green = parse_hex_channel(text, kGreenChannel);
  const std::optional<float> blue = parse_hex_channel(text, kBlueChannel);
  const bool all_read = red && green && blue;
  if (!all_read) return std::nullopt;
  return Vec3{*red, *green, *blue};
}

}  // namespace redgem
