#include "project/component_words.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <sstream>

#include "core/config.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "project/scene_text.hpp"
#include "project/tool_line.hpp"
#include "scene/property.hpp"

namespace redgem::tools::component_words {
namespace {

constexpr LightType kLightTypes[] = {LightType::Directional, LightType::Point, LightType::Spot};
constexpr char kComma = ',';
constexpr std::size_t kChannels = 3;
constexpr double kDarkest = 0.0;  // a color channel's range, as the Inspector's picker takes it
constexpr double kBrightest = 1.0;
constexpr float kOpaque = 1.0f;
constexpr double kNone = 0.0;  // intensities, ranges and the far plane are never negative
constexpr int kShownDecimals = 3;
constexpr const char* kLightKey = "light";
constexpr const char* kColorKey = "light_color";
constexpr const char* kIntensityKey = "light_intensity";
constexpr const char* kRangeKey = "light_range";
constexpr const char* kFovKey = "camera_fov";
constexpr const char* kNearKey = "camera_near";
constexpr const char* kFarKey = "camera_far";

// A number from `least` to `most`, or what is wrong with the word.
struct Bounds {
  double least;
  double most;
};

std::optional<double> number_within(const std::string& value, Bounds bounds) {
  const auto number = number_from(value);
  if (!number || !std::isfinite(*number) || *number < bounds.least || *number > bounds.most) return std::nullopt;
  return number;
}

std::string out_of_bounds(const char* key, const std::string& value, Bounds bounds) {
  return tr("project.scene.needs_number_in", {{"key", key},
                                              {"value", value},
                                              {"least", number_text(bounds.least, kShownDecimals)},
                                              {"most", number_text(bounds.most, kShownDecimals)}});
}

std::string needs(const char* text_key, const char* key) { return tr(text_key, {{"key", key}}); }

std::optional<Color> rgb_from(const std::string& value) {
  std::array<double, kChannels> channels{};
  std::stringstream stream(value);
  std::size_t count = 0;
  for (std::string part; std::getline(stream, part, kComma); ++count) {
    const auto channel = count < kChannels ? number_within(part, {kDarkest, kBrightest}) : std::nullopt;
    if (!channel) return std::nullopt;
    channels[count] = *channel;
  }
  if (count != kChannels) return std::nullopt;
  const auto [red, green, blue] = channels;
  return Color{static_cast<float>(red), static_cast<float>(green), static_cast<float>(blue), kOpaque};
}

std::optional<Color> color_from(const std::string& value) {
  if (const auto hex = parse_hex_color(value)) return Color{hex->x, hex->y, hex->z, kOpaque};
  return rgb_from(value);
}

// Changes the object's light with `change`, which says what is wrong with the value (empty when
// nothing is); an object without a light has nothing to change.
template <typename Change>
std::string change_light(SceneDocument& document, int index, const char* key, Change change) {
  const std::optional<LightDesc>& original = document.node(index).light;
  if (!original) return needs("project.scene.needs_light", key);
  LightDesc light = *original;
  const std::string problem = change(light);
  if (problem.empty()) document.set_light(index, light);
  return problem;
}

template <typename Change>
std::string change_camera(SceneDocument& document, int index, const char* key, Change change) {
  const std::optional<CameraDesc>& original = document.node(index).camera;
  if (!original) return needs("project.scene.needs_camera", key);
  CameraDesc camera = *original;
  const std::string problem = change(camera);
  if (problem.empty()) document.set_camera(index, camera);
  return problem;
}

// Sets `field` of a light to the value, a number within `bounds`.
std::string light_number(SceneDocument& document, int index, const char* key, const std::string& value,
                         float LightDesc::* field, Bounds bounds) {
  return change_light(document, index, key, [&](LightDesc& light) -> std::string {
    const auto number = number_within(value, bounds);
    if (!number) return out_of_bounds(key, value, bounds);
    light.*field = static_cast<float>(*number);
    return {};
  });
}

std::string camera_number(SceneDocument& document, int index, const char* key, const std::string& value,
                          float CameraDesc::* field, Bounds bounds) {
  return change_camera(document, index, key, [&](CameraDesc& camera) -> std::string {
    const auto number = number_within(value, bounds);
    if (!number) return out_of_bounds(key, value, bounds);
    camera.*field = static_cast<float>(*number);
    return {};
  });
}

std::optional<LightType> light_type_named(const std::string& value) {
  for (const LightType type : kLightTypes) {
    if (value == light_name(type)) return type;
  }
  return std::nullopt;
}

// An object without a light takes `light=` with nothing after it back, as a line says it.
bool says_no_light(const SceneDocument& document, int index, const std::string& value) {
  return !document.node(index).light && value.empty();
}

}  // namespace

std::string set_light_type(SceneDocument& document, int index, const std::string& value) {
  if (says_no_light(document, index, value)) return {};
  return change_light(document, index, kLightKey, [&value](LightDesc& light) -> std::string {
    const auto type = light_type_named(value);
    if (!type) return tr("project.scene.light_kinds", {{"value", value}});
    light.type = *type;
    sanitize_spot_cones(light);
    return {};
  });
}

std::string set_light_color(SceneDocument& document, int index, const std::string& value) {
  return change_light(document, index, kColorKey, [&value](LightDesc& light) -> std::string {
    const auto color = color_from(value);
    if (!color) return tr("project.scene.needs_color", {{"key", kColorKey}, {"value", value}});
    light.color = *color;
    return {};
  });
}

std::string set_light_intensity(SceneDocument& document, int index, const std::string& value) {
  return light_number(document, index, kIntensityKey, value, &LightDesc::intensity,
                      {kNone, config::kLargestCoordinate});
}

std::string set_light_range(SceneDocument& document, int index, const std::string& value) {
  return light_number(document, index, kRangeKey, value, &LightDesc::range, {kNone, config::kLargestCoordinate});
}

std::string set_camera_fov(SceneDocument& document, int index, const std::string& value) {
  const Bounds degrees{editor_config::kMinFovDegrees, editor_config::kMaxFovDegrees};
  return change_camera(document, index, kFovKey, [&](CameraDesc& camera) -> std::string {
    const auto number = number_within(value, degrees);
    if (!number) return out_of_bounds(kFovKey, value, degrees);
    camera.fov_y_radians = degrees_to_radians(static_cast<float>(*number));
    return {};
  });
}

std::string set_camera_near(SceneDocument& document, int index, const std::string& value) {
  return camera_number(document, index, kNearKey, value, &CameraDesc::near_plane,
                       {editor_config::kMinClipDistance, config::kLargestCoordinate});
}

std::string set_camera_far(SceneDocument& document, int index, const std::string& value) {
  return camera_number(document, index, kFarKey, value, &CameraDesc::far_plane, {kNone, config::kLargestCoordinate});
}

}  // namespace redgem::tools::component_words
