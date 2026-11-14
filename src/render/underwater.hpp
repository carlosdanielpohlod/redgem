#pragma once

// Seeing from under the water: fog, tint and a slow wobble over the picture. How far under is
// smoothed across a band around the surface, so the view fades in and out as the camera crosses
// it instead of switching (the waves move the surface it sees; a hard switch would flicker).

#include "core/math.hpp"

namespace redgem {

inline constexpr float kUnderwaterMinBand = 0.05f;  // meters: the crossing blend's half width on calm water

struct UnderwaterView {
  bool water = false;  // whether the camera is over or in any water at all
  float depth = 0.0f;  // meters below the undisplaced surface (negative above it)
  float band = kUnderwaterMinBand;
  Vec3 absorption;  // per meter, the surface's material's
};

// How much of the underwater look the picture gets: 0 above the band, 1 below it, smooth between.
float underwater_amount(const UnderwaterView& view);
// The half width a surface with waves that tall is crossed over.
float underwater_band(float wave_height);

}  // namespace redgem
