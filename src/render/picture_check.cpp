#include "render/picture_check.hpp"

namespace redgem {

std::size_t see_through_pixels(const std::uint8_t* rgba, std::size_t pixels) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < pixels; ++i) {
    if (rgba[i * kRgbaChannels + kAlphaChannel] != kOpaque) ++count;
  }
  return count;
}

bool unfinished(const std::uint8_t* rgba, std::size_t pixels) {
  const auto seen_through = static_cast<double>(see_through_pixels(rgba, pixels));
  return seen_through > kUnfinishedShare * static_cast<double>(pixels);
}

}  // namespace redgem
