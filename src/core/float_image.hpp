#pragma once

// A picture of linear light, brighter than white allowed: what a sky picture becomes once it is
// decoded, whatever file it came from.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/math.hpp"

namespace redgem {

inline constexpr std::size_t kChannelsPerPixel = 3;  // red, green, blue

// Red, green and blue floats per pixel, row by row from the top.
struct FloatImage {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<float> rgb;

  bool empty() const { return width == 0 || height == 0; }
  std::size_t index(std::uint32_t x, std::uint32_t y) const {
    return (static_cast<std::size_t>(y) * width + x) * kChannelsPerPixel;
  }
  Vec3 at(std::uint32_t x, std::uint32_t y) const { return vec3_from(&rgb[index(x, y)]); }
};

// A picture `width` x `height`, black.
FloatImage black_image(std::uint32_t width, std::uint32_t height);

// The smallest power of two that a side `side` pixels long is divided by to be at most `widest`.
std::uint32_t shrink_to_fit(std::uint32_t side, std::uint32_t widest);

// Adds `share` of `light` to the pixel at (x, y).
void add_share(FloatImage& image, std::uint32_t x, std::uint32_t y, Vec3 light, float share);

// Every `shrink` x `shrink` block of pixels averaged into one (what is left over at the right and
// bottom edges is dropped). `shrink` 1 returns the picture as it is. Never past a pixel along
// either side: a picture far wider than tall is shrunk only as much as its height allows.
FloatImage shrunk(const FloatImage& image, std::uint32_t shrink);

}  // namespace redgem
