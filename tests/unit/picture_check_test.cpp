#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "render/picture_check.hpp"

using namespace redgem;

namespace {

constexpr std::uint8_t kGrey = 128;
constexpr std::uint8_t kHalfSeen = 100;

std::vector<std::uint8_t> opaque_grey(std::size_t pixels) {
  std::vector<std::uint8_t> rgba;
  for (std::size_t i = 0; i < pixels; ++i) rgba.insert(rgba.end(), {kGrey, kGrey, kGrey, kOpaque});
  return rgba;
}

}  // namespace

TEST_CASE("a picture drawn whole has no see-through pixels") {
  constexpr std::size_t kPixels = 16;
  const auto rgba = opaque_grey(kPixels);
  CHECK(see_through_pixels(rgba.data(), kPixels) == 0);
}

TEST_CASE("the see-through pixels of a picture the card did not finish are counted") {
  constexpr std::size_t kPixels = 16;
  constexpr std::size_t kFirstBroken = 5;
  auto rgba = opaque_grey(kPixels);
  rgba[kFirstBroken * kRgbaChannels + kAlphaChannel] = 0;
  rgba[(kFirstBroken + 1) * kRgbaChannels + kAlphaChannel] = kHalfSeen;
  CHECK(see_through_pixels(rgba.data(), kPixels) == 2);
}

TEST_CASE("a picture with a HUD's text over it is whole; one see-through over much of it is not") {
  constexpr std::size_t kPixels = 100;
  constexpr std::size_t kHudPixels = 5;       // text in a corner
  constexpr std::size_t kDroppedPixels = 40;  // the least of the dropped frames seen
  auto rgba = opaque_grey(kPixels);
  for (std::size_t i = 0; i < kHudPixels; ++i) rgba[i * kRgbaChannels + kAlphaChannel] = kHalfSeen;
  CHECK_FALSE(unfinished(rgba.data(), kPixels));
  for (std::size_t i = 0; i < kDroppedPixels; ++i) rgba[i * kRgbaChannels + kAlphaChannel] = 0;
  CHECK(unfinished(rgba.data(), kPixels));
}
