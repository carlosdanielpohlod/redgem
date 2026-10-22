#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "scene/scene.hpp"

namespace redgem {

inline constexpr std::size_t kMaxWaterRipples = 16;      // bounded work in every water shader pass
inline constexpr float kWaterRippleLifetime = 3.0f;      // simulation seconds until an impulse expires
inline constexpr float kWaterRippleMaxStrength = 0.18f;  // maximum displacement, in world meters
inline constexpr float kWaterRippleWavelength = 0.65f;   // world meters between radial crests
inline constexpr float kWaterRippleMinRadius = 0.15f;    // world meters: avoid singular, subpixel packets
inline constexpr float kWaterRippleMaxRadius = 1.5f;
inline constexpr float kWaterRippleDefaultRadius = 0.2f;
inline constexpr float kWaterRippleSpeed = 1.8f;  // world meters per simulation second

struct WaterRipple {
  std::uint64_t serial = 0;  // monotonic within this simulation; matches previous-frame impulses
  ObjectHandle surface;
  std::size_t primitive = 0;                 // original MeshDesc::primitives index, before GPU splitting
  Vec3 origin;                               // world coordinates, on the undisplaced horizontal surface
  float age = 0.0f;                          // simulation seconds; drawing never advances it
  float strength = 0.0f;                     // world meters, nonnegative and bounded by kWaterRippleMaxStrength
  float radius = kWaterRippleDefaultRadius;  // initial width of the wave packet, in world meters
};

struct WaterRippleSnapshot {
  std::array<WaterRipple, kMaxWaterRipples> ripples{};
  std::size_t count = 0;
  double time = 0.0;  // local simulation clock, reset with the world; independent of renderer time
};

}  // namespace redgem
