#pragma once

// How the copies of a Scatter set differ from each other besides where they stand: a color
// each, picked at random from a range of colors and made a little brighter or darker, so that a
// meadow is not one green. The same for the same seed on every machine.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/math.hpp"

namespace redgem {

inline constexpr std::size_t kMaxScatterTints = 16;  // colors a range may run through

struct ScatterLooks {
  // Each copy takes a color at random along this list (between two neighbours of it, blended);
  // empty, they keep the template's.
  std::vector<Vec3> tints;
  float tint_variation = 0.0f;  // 0..1: each copy brighter or darker by up to this share
};

// Whether the copies look different from each other at all.
bool varied(const ScatterLooks& looks);

// The color of each of `count` copies (multiplying the template's tint), from dice of their own:
// where copies stand stays the same whatever they look like. Empty when nothing varies.
std::vector<Vec3> scatter_tints(const ScatterLooks& looks, std::uint32_t seed, std::size_t count);

}  // namespace redgem
