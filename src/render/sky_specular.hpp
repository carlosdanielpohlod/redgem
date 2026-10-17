#pragma once

// What glossy surfaces reflect of a sky picture, worked out once when it is loaded, the
// "split sum" way: the sky blurred for each roughness (a cube map, one roughness a mip level), and
// a table of how much of it a surface reflects by roughness and viewing angle. No window and no
// graphics card needed, so it can be tested.

#include <cstdint>
#include <vector>

#include "core/float_image.hpp"
#include "core/math.hpp"

namespace redgem::render_math {

inline constexpr int kSkyCubeSize = 128;          // texels along a face of the sharpest level
inline constexpr int kSkyCubeLevels = 8;          // 128 down to 1: the graphics card wants them all
inline constexpr int kSkyRoughestLevel = 5;       // the level of roughness 1 (the ones below repeat it)
inline constexpr int kReflectanceTableSize = 32;  // texels along each side of the table

// The direction of a reflection off a surface of `roughness` (0..1), taken at random by GGX
// around `normal` for two numbers 0..1, and how likely it was.
struct SampledReflection {
  Vec3 direction;
  float likelihood = 0.0f;  // per unit of solid angle
};
SampledReflection sample_reflection(Vec3 normal, float roughness, float first, float second);
// The i-th of `count` evenly spread pairs of numbers 0..1 (Hammersley).
void spread_pair(std::uint32_t i, std::uint32_t count, float& first, float& second);

// The sky's cube map, as half floats (four per texel), face after face, level after level.
// `pyramid` is the picture, then each smaller level of it halved again (see picture_pyramid).
std::vector<std::uint16_t> sky_reflection_cube(const std::vector<FloatImage>& pyramid);
std::vector<FloatImage> picture_pyramid(const FloatImage& picture);

// How much of the blurred sky a surface reflects: per texel, what multiplies its base
// reflectance (red) and what adds to it (green), for the cosine of the viewing angle across
// (0..1) and roughness down (0..1). Half floats, four per texel.
std::vector<std::uint16_t> reflectance_table();
// One entry of it, as floats: {scale, bias}.
Vec3 reflectance(float cos_view, float roughness);

}  // namespace redgem::render_math
