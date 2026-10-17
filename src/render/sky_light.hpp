#pragma once

// Everything the renderer keeps of a sky picture, worked out once when it is loaded: the picture
// itself for the sky behind everything, the light it gives matte surfaces (sky_harmonics.hpp) and
// what glossy ones reflect of it (sky_specular.hpp). No window and no graphics card needed, so it
// can be tested.

#include <cstdint>
#include <vector>

#include "core/float_image.hpp"
#include "render/sky_harmonics.hpp"

namespace redgem::render_math {

inline constexpr std::uint32_t kSkyPictureWidest = 4096;  // pixels of the sky drawn behind everything
inline constexpr std::uint32_t kSkyLightWidest = 512;     // pixels of the picture the light is worked out from
// The brightest a pixel of the picture lights the scene with (linear, its brightest channel): a
// sun in the picture would otherwise light everything a second time, on top of the scene's sun.
inline constexpr float kBrightestSkyLight = 32.0f;

struct SkyLight {
  SkyHarmonics irradiance{};
  std::vector<std::uint16_t> reflections;  // sky_reflection_cube
};

// What lights the scene from a picture of the sky (of any size: it is shrunk first).
SkyLight sky_light_of(const FloatImage& picture);

// The picture with no pixel brighter than kBrightestSkyLight, its colors kept.
FloatImage held_to_sky_light(FloatImage picture);

// The picture as half floats, four per texel, row by row from the top.
std::vector<std::uint16_t> picture_texels(const FloatImage& picture);

}  // namespace redgem::render_math
