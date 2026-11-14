#pragma once

// Where the water is: every water surface fills the world column under its footprint, down to
// whatever ground the level has there. Heights are the undisplaced surface (waves are not counted).

#include <optional>
#include <vector>

#include "scene/water_geometry.hpp"

namespace redgem {

// The water over a world column: the highest surface whose footprint covers it.
struct WaterColumn {
  float height = 0.0f;                    // world meters
  const WaterSurface* surface = nullptr;  // good until the surfaces are found again
};

std::optional<WaterColumn> water_column_at(const std::vector<WaterSurface>& surfaces, float x, float z);
// Whether a point is below the water surface of its column.
bool water_under(const std::vector<WaterSurface>& surfaces, Vec3 point);
// How far below its column's surface a point is (negative above it), or nothing outside any water.
std::optional<float> water_depth(const std::vector<WaterSurface>& surfaces, Vec3 point);

}  // namespace redgem
