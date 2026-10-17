#pragma once

// The light a sky picture gives a surface from all around it (its irradiance), kept as nine
// spherical harmonics: the picture is projected onto them once, when it is loaded, and the lit
// shader sums them for each surface's normal (shaders/sky_light.hpp). No window and no graphics
// card needed, so it can be tested.

#include <array>

#include "core/float_image.hpp"
#include "core/math.hpp"

namespace redgem::render_math {

inline constexpr int kSkyHarmonics = 9;  // bands 0, 1 and 2
using SkyHarmonics = std::array<Vec3, kSkyHarmonics>;

// The nine basis functions in a direction (unit length), in the order the shader sums them.
std::array<float, kSkyHarmonics> harmonic_basis(Vec3 direction);

// What a surface facing each way receives from the picture, divided by pi: summed over the basis
// for a normal, it is the light a white matte surface sends back (1 under a sky of 1 all around),
// the same measure as the gradient sky the lit shader falls back on.
SkyHarmonics sky_irradiance(const FloatImage& picture);

// The harmonics as the shader's array of vec3 takes them.
using HarmonicFloats = std::array<float, kSkyHarmonics * kChannelsPerPixel>;  // red, green and blue of each
HarmonicFloats harmonic_floats(const SkyHarmonics& harmonics);

// That sum, as the shader works it out.
Vec3 irradiance_toward(const SkyHarmonics& harmonics, Vec3 normal);

}  // namespace redgem::render_math
