#pragma once

// Seeing from under the water: fog, tint and a slow wobble over the picture. How far under is
// smoothed across a band from just above the calm surface down to the waves' height under it, so
// the view fades in as the camera goes under instead of switching (the waves move the surface it
// sees; a hard switch would flicker). A camera above the calm surface keeps a clear picture.

#include "core/math.hpp"

namespace redgem {

inline constexpr float kUnderwaterMinBand = 0.05f;  // meters: how far above the calm surface the blend starts

struct UnderwaterView {
  bool water = false;               // whether the camera is over or in any water at all
  float depth = 0.0f;               // meters below the undisplaced surface (negative above it)
  float band = kUnderwaterMinBand;  // meters under the calm surface where the look is whole
  Vec3 absorption;                  // per meter, the surface's material's
};

// How much of the underwater look the picture gets: 0 above the band, 1 below it, smooth between.
float underwater_amount(const UnderwaterView& view);
// How deep under a surface with waves that tall the look becomes whole.
float underwater_band(float wave_height);

}  // namespace redgem
