#include "render/display_settings.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace redgem {
namespace {

using ModeName = std::pair<WindowMode, const char*>;
constexpr ModeName kModeNames[] = {
    {WindowMode::Windowed, "windowed"}, {WindowMode::Fullscreen, "fullscreen"}, {WindowMode::Borderless, "borderless"}};

// What the settings are kept under (see display_json).
constexpr const char* kModeKey = "window_mode";
constexpr const char* kWidthKey = "width";
constexpr const char* kHeightKey = "height";
constexpr const char* kRenderScaleKey = "render_scale";
constexpr const char* kVsyncKey = "vsync";
constexpr const char* kFpsCapKey = "fps_cap";
constexpr const char* kQualityKey = "quality";
constexpr const char* kCustomQualityKey = "custom_quality";
constexpr const char* kVolumeKey = "volume";
constexpr const char* kMsaaKey = "msaa";
constexpr const char* kAntialiasKey = "antialias";
constexpr const char* kCascadesKey = "shadow_cascades";
constexpr const char* kShadowDistanceKey = "shadow_distance";
constexpr const char* kScatterDensityKey = "scatter_density";
constexpr const char* kScatterDistanceKey = "scatter_distance";
constexpr const char* kSkyDetailKey = "sky_detail";
constexpr const char* kOcclusionKey = "ambient_occlusion";
constexpr const char* kBloomKey = "bloom";

constexpr float kHalfPixel = 0.5f;  // (rounding to the nearest pixel)

// The window sizes players usually pick, smallest first.
constexpr SurfaceSize kUsualSizes[] = {{640, 360},  {800, 600},   {1024, 768},  {1280, 720}, {1366, 768},
                                       {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};

bool known_size(SurfaceSize monitor) { return monitor.width > 0 && monitor.height > 0; }

// Whether a window of that size fits on the monitor; on one whose size is not known, every size does.
bool fits_in(SurfaceSize size, SurfaceSize monitor) {
  return !known_size(monitor) || (size.width <= monitor.width && size.height <= monitor.height);
}

// Whether a usual size is offered besides the monitor's own: it fits, and it is not that one.
bool offered_besides(SurfaceSize size, SurfaceSize monitor) { return fits_in(size, monitor) && size != monitor; }

// A size times `scale`, rounded to whole pixels, never less than one a side.
SurfaceSize scaled_size_by(SurfaceSize size, float scale) {
  const auto side = [scale](int length) {
    return std::max(1, static_cast<int>(std::floor(static_cast<float>(length) * scale + kHalfPixel)));
  };
  return {side(size.width), side(size.height)};
}

bool holds(const nlohmann::json& json, const char* key, const std::function<bool(const nlohmann::json&)>& kind) {
  return json.contains(key) && kind(json[key]);
}

bool is_bool(const nlohmann::json& value) { return value.is_boolean(); }
bool is_number(const nlohmann::json& value) { return value.is_number() && std::isfinite(value.get<double>()); }
bool is_text(const nlohmann::json& value) { return value.is_string(); }

void read_bool(const nlohmann::json& json, const char* key, bool& into) {
  if (holds(json, key, is_bool)) into = json[key].get<bool>();
}

void read_share(const nlohmann::json& json, const char* key, float& into) {
  if (holds(json, key, is_number)) into = std::clamp(json[key].get<float>(), 0.0f, 1.0f);
}

// A whole number held to `low`..`high` (one with a fraction, as a script's slider gives, rounded).
// (Held to the range before it becomes an int: a number past an int's range does not fit one.)
void read_whole(const nlohmann::json& json, const char* key, int low, int high, int& into) {
  if (holds(json, key, is_number)) {
    into = static_cast<int>(
        std::lround(std::clamp(json[key].get<double>(), static_cast<double>(low), static_cast<double>(high))));
  }
}

template <typename T>
void read_name(const nlohmann::json& json, const char* key, std::optional<T> (*from_name)(const std::string&),
               T& into) {
  if (holds(json, key, is_text)) into = from_name(json[key].get<std::string>()).value_or(into);
}

nlohmann::json caps_json(const QualityCaps& caps) {
  return {{kMsaaKey, caps.msaa},
          {kAntialiasKey, caps.temporal_antialias},
          {kCascadesKey, caps.shadow_cascades},
          {kShadowDistanceKey, caps.shadow_distance},
          {kScatterDensityKey, caps.scatter_density},
          {kScatterDistanceKey, caps.scatter_distance},
          {kSkyDetailKey, caps.sky_detail},
          {kOcclusionKey, caps.ambient_occlusion},
          {kBloomKey, caps.bloom}};
}

void read_caps(const nlohmann::json& json, QualityCaps& caps) {
  if (!json.is_object()) return;
  read_whole(json, kMsaaKey, 0, config::kMostSamples, caps.msaa);
  read_bool(json, kAntialiasKey, caps.temporal_antialias);
  read_whole(json, kCascadesKey, 1, config::kMostCascades, caps.shadow_cascades);
  using Share = std::pair<const char*, float*>;
  const Share shares[] = {{kShadowDistanceKey, &caps.shadow_distance},   {kScatterDensityKey, &caps.scatter_density},
                          {kScatterDistanceKey, &caps.scatter_distance}, {kSkyDetailKey, &caps.sky_detail},
                          {kOcclusionKey, &caps.ambient_occlusion},      {kBloomKey, &caps.bloom}};
  for (const auto& [key, share] : shares) read_share(json, key, *share);
}

void read_window(const nlohmann::json& json, DisplaySettings& display) {
  read_name(json, kModeKey, window_mode_from_name, display.mode);
  read_whole(json, kWidthKey, config::kMinWindowSide, config::kMaxWindowSide, display.resolution.width);
  read_whole(json, kHeightKey, config::kMinWindowSide, config::kMaxWindowSide, display.resolution.height);
  if (holds(json, kRenderScaleKey, is_number))
    display.render_scale = held_render_scale(json[kRenderScaleKey].get<float>());
  read_bool(json, kVsyncKey, display.vsync);
  read_whole(json, kFpsCapKey, kNoFrameCap, kMostFramesPerSecond, display.fps_cap);
}

}  // namespace

const char* window_mode_name(WindowMode mode) {
  for (const auto& [known, name] : kModeNames) {
    if (known == mode) return name;
  }
  return kModeNames[0].second;  // (every mode has its name)
}

std::optional<WindowMode> window_mode_from_name(const std::string& name) {
  for (const auto& [mode, known] : kModeNames) {
    if (name == known) return mode;
  }
  return std::nullopt;
}

QualityCaps quality_caps(const DisplaySettings& display) {
  return display.quality == QualityPreset::Custom ? display.custom_quality : preset_caps(display.quality);
}

float held_render_scale(float scale) {
  return std::isfinite(scale) ? std::clamp(scale, kSmallestRenderScale, kLargestRenderScale) : kFullRenderScale;
}

// (A scale held to its range is exactly 1 when it was asked to be: it is never off by a rounding.)
bool scales_picture(float render_scale) { return held_render_scale(render_scale) != kFullRenderScale; }

SurfaceSize scaled_size(SurfaceSize window, float render_scale) {
  return scaled_size_by(window, held_render_scale(render_scale));
}

SurfaceSize held_to_side(SurfaceSize size, int largest) {
  const int longest = std::max(size.width, size.height);
  if (longest <= largest) return size;
  const float shrink = static_cast<float>(largest) / static_cast<float>(longest);
  return scaled_size_by(size, shrink);
}

bool same_shape(const DisplaySettings& a, const DisplaySettings& b) {
  return a.mode == b.mode && a.resolution == b.resolution;
}

bool same_pace(const DisplaySettings& a, const DisplaySettings& b) {
  return a.vsync == b.vsync && a.fps_cap == b.fps_cap;
}

int held_fps_cap(int cap) { return std::clamp(cap, kNoFrameCap, kMostFramesPerSecond); }

nlohmann::json display_json(const DisplaySettings& display) {
  return {{kModeKey, window_mode_name(display.mode)},
          {kWidthKey, display.resolution.width},
          {kHeightKey, display.resolution.height},
          {kRenderScaleKey, display.render_scale},
          {kVsyncKey, display.vsync},
          {kFpsCapKey, display.fps_cap},
          {kQualityKey, quality_name(display.quality)},
          {kCustomQualityKey, caps_json(display.custom_quality)},
          {kVolumeKey, display.volume}};
}

DisplaySettings display_from_json(const nlohmann::json& json, DisplaySettings base) {
  if (!json.is_object()) return base;
  read_window(json, base);
  read_name(json, kQualityKey, quality_from_name, base.quality);
  if (json.contains(kCustomQualityKey)) read_caps(json[kCustomQualityKey], base.custom_quality);
  read_share(json, kVolumeKey, base.volume);
  return base;
}

std::vector<SurfaceSize> window_sizes_for(SurfaceSize monitor) {
  std::vector<SurfaceSize> sizes;
  for (const SurfaceSize size : kUsualSizes) {
    if (offered_besides(size, monitor)) sizes.push_back(size);
  }
  if (known_size(monitor)) sizes.push_back(monitor);
  return sizes;
}

}  // namespace redgem
