#pragma once

// Colors as people pick them and as light adds up. A color picker, "#rrggbb" and a script show a
// channel as sRGB, from 0 to 255; the engine keeps it linear, from 0 to 1 (brighter for lights).

#include <cmath>

#include "scene/scene_config.hpp"

namespace redgem {

inline constexpr float kSrgbGamma = 2.2f;  // the plain curve, close to sRGB's own
inline constexpr float kMaxColorChannel = static_cast<float>(config::kMaxColorByte);  // as a picker shows it

// A channel as a picker shows it (0 to 1) in linear light, and back.
inline float linear_from_srgb(float shown) { return std::pow(shown, kSrgbGamma); }
inline float srgb_from_linear(float linear) { return std::pow(linear, 1.0f / kSrgbGamma); }

}  // namespace redgem
