#include "render/sky_light.hpp"

#include <algorithm>

#include "render/half_float.hpp"
#include "render/sky_specular.hpp"

namespace redgem::render_math {

FloatImage held_to_sky_light(FloatImage picture) {
  for (std::size_t at = 0; at < picture.rgb.size(); at += kChannelsPerPixel) {
    float* pixel = &picture.rgb[at];
    const float brightest = std::max({pixel[kXOffset], pixel[kYOffset], pixel[kZOffset]});
    if (brightest <= kBrightestSkyLight) continue;
    const float scale = kBrightestSkyLight / brightest;
    for (std::size_t c = 0; c < kChannelsPerPixel; ++c) pixel[c] *= scale;
  }
  return picture;
}

namespace {

// The picture as it lights the scene: shrunk and held down; a picture with no pixels, a black sky.
FloatImage lighting_picture(const FloatImage& picture) {
  if (picture.empty()) return black_image(1, 1);
  return held_to_sky_light(shrunk(picture, shrink_to_fit(picture.width, kSkyLightWidest)));
}

}  // namespace

SkyLight sky_light_of(const FloatImage& picture) {
  const FloatImage light = lighting_picture(picture);
  return {sky_irradiance(light), sky_reflection_cube(picture_pyramid(light))};
}

std::vector<std::uint16_t> picture_texels(const FloatImage& picture) {
  std::vector<std::uint16_t> texels;
  texels.reserve(static_cast<std::size_t>(picture.width) * picture.height * kHalfsPerTexel);
  for (std::uint32_t y = 0; y < picture.height; ++y) {
    for (std::uint32_t x = 0; x < picture.width; ++x) add_texel(texels, picture.at(x, y));
  }
  return texels;
}

}  // namespace redgem::render_math
