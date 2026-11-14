#include "render/auto_exposure.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

template <typename Value>
void take(const std::optional<Value>& override_value, Value& out) {
  if (override_value) out = *override_value;
}

// Whether the scene got brighter than the exposure is adapted to.
bool brightening(float current, float target) { return target > current; }

}  // namespace

AutoExposure with_overrides(AutoExposure settings, const AutoExposureOverrides& overrides) {
  take(overrides.enabled, settings.enabled);
  take(overrides.min_ev, settings.min_ev);
  take(overrides.max_ev, settings.max_ev);
  take(overrides.speed_up, settings.speed_up);
  take(overrides.speed_down, settings.speed_down);
  take(overrides.compensation, settings.compensation);
  settings.max_ev = std::max(settings.max_ev, settings.min_ev);
  return settings;
}

namespace render_math {

float target_ev(float mean_log2, const AutoExposure& settings) {
  const float ev = std::isfinite(mean_log2) ? mean_log2 - kMiddleGreyLog2 : 0.0f;
  return std::clamp(ev, settings.min_ev, std::max(settings.max_ev, settings.min_ev));
}

float adapted_ev(float current, float target, const AutoExposure& settings, float seconds) {
  const float step = std::clamp(seconds, 0.0f, kLongestAdaptStep);
  const float speed = brightening(current, target) ? settings.speed_up : settings.speed_down;
  const float closed = 1.0f - std::exp(-std::max(speed, 0.0f) * step);
  return current + (target - current) * closed;
}

float exposure_scale(float ev, float compensation) { return std::exp2(compensation - ev); }

bool starts_anew(const ExposureHistory& history, std::uint64_t viewer, std::uint64_t scene) {
  return !history.adapted || history.viewer != viewer || history.scene != scene;
}

float adapt_seconds(const ExposureHistory& history, float now) {
  return std::clamp(now - history.time, 0.0f, kLongestAdaptStep);
}

}  // namespace render_math
}  // namespace redgem
