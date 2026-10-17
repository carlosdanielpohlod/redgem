#include "core/float_image.hpp"

#include <algorithm>

namespace redgem {
namespace {

constexpr std::uint32_t kHalvings = 2;  // a shrink is a power of two

// The largest shrink that leaves a side `side` pixels long at least one pixel (1 for none).
std::uint32_t largest_shrink_of(std::uint32_t side) {
  std::uint32_t shrink = 1;
  while (shrink * kHalvings <= side) shrink *= kHalvings;
  return shrink;
}

}  // namespace

FloatImage black_image(std::uint32_t width, std::uint32_t height) {
  FloatImage image;
  image.width = width;
  image.height = height;
  image.rgb.assign(static_cast<std::size_t>(width) * height * kChannelsPerPixel, 0.0f);
  return image;
}

std::uint32_t shrink_to_fit(std::uint32_t side, std::uint32_t widest) {
  std::uint32_t shrink = 1;
  while (side / shrink > widest) shrink *= kHalvings;
  return shrink;
}

void add_share(FloatImage& image, std::uint32_t x, std::uint32_t y, Vec3 light, float share) {
  float* pixel = &image.rgb[image.index(x, y)];
  pixel[kXOffset] += light.x * share;
  pixel[kYOffset] += light.y * share;
  pixel[kZOffset] += light.z * share;
}

FloatImage shrunk(const FloatImage& image, std::uint32_t asked) {
  const std::uint32_t shrink = std::min(asked, largest_shrink_of(std::min(image.width, image.height)));
  if (shrink <= 1) return image;
  FloatImage small = black_image(image.width / shrink, image.height / shrink);
  const float share = 1.0f / static_cast<float>(shrink * shrink);
  for (std::uint32_t y = 0; y < small.height * shrink; ++y) {
    for (std::uint32_t x = 0; x < small.width * shrink; ++x)
      add_share(small, x / shrink, y / shrink, image.at(x, y), share);
  }
  return small;
}

}  // namespace redgem
