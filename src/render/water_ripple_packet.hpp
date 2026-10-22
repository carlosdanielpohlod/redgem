#pragma once

#include <raylib.h>

#include <array>

#include "scene/water_ripples.hpp"

namespace redgem::render_detail {

// Previous geometry retains expired impulses and initializes newly born ones without false motion.
inline constexpr std::size_t kMaxDrawnWaterRipples = kMaxWaterRipples * 2;

struct WaterRipplePacket {
  std::array<Vector4, kMaxDrawnWaterRipples> waves{};  // world X/Z, simulation age, strength
  std::array<float, kMaxDrawnWaterRipples> radii{};
  int count = 0;
};

WaterRippleSnapshot validated_water_ripples(const WaterRippleSnapshot* snapshot);
WaterRipplePacket water_ripple_packet(const WaterRippleSnapshot& snapshot, ObjectHandle surface, std::size_t primitive);
WaterRipplePacket previous_water_ripple_packet(const WaterRippleSnapshot& previous, const WaterRippleSnapshot& current,
                                               ObjectHandle surface, std::size_t primitive);
float water_ripple_bound(const WaterRippleSnapshot& snapshot, ObjectHandle surface);

}  // namespace redgem::render_detail
