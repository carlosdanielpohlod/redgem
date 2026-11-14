#include "render/underwater.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr float kSmoothStepSquare = 3.0f;  // smoothstep: 3t^2 - 2t^3
constexpr float kSmoothStepCube = 2.0f;

bool usable(const UnderwaterView& view) { return view.water && std::isfinite(view.depth) && view.band > 0.0f; }

}  // namespace

float underwater_amount(const UnderwaterView& view) {
  if (!usable(view)) return 0.0f;
  const float t = std::clamp((view.depth + kUnderwaterMinBand) / (view.band + kUnderwaterMinBand), 0.0f, 1.0f);
  return t * t * (kSmoothStepSquare - kSmoothStepCube * t);
}

float underwater_band(float wave_height) {
  if (!std::isfinite(wave_height)) return kUnderwaterMinBand;
  return std::max(kUnderwaterMinBand, std::fabs(wave_height));
}

}  // namespace redgem
