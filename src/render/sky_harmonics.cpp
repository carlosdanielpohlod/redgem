#include "render/sky_harmonics.hpp"

#include <cmath>

#include "render/sky_directions.hpp"

namespace redgem::render_math {
namespace {

// The real spherical harmonics' constants (bands 0, 1 and 2).
constexpr float kBand0 = 0.282095f;
constexpr float kBand1 = 0.488603f;
constexpr float kBand2Cross = 1.092548f;
constexpr float kBand2Zonal = 0.315392f;
constexpr float kBand2Square = 0.546274f;
constexpr float kThree = 3.0f;
// How a matte surface blurs each band (the cosine lobe's own harmonics), already divided by pi.
constexpr float kLobe0 = 1.0f;
constexpr float kLobe1 = 2.0f / 3.0f;
constexpr float kLobe2 = 0.25f;
constexpr std::array<float, kSkyHarmonics> kLobes = {kLobe0, kLobe1, kLobe1, kLobe1, kLobe2,
                                                     kLobe2, kLobe2, kLobe2, kLobe2};

// The solid angle a pixel of row `y` covers: rows near the poles are squeezed.
float pixel_solid_angle(const FloatImage& picture, std::uint32_t y) {
  const float latitude = (kHalf - (static_cast<float>(y) + kHalf) / static_cast<float>(picture.height)) * kPi;
  const float across = kTwo * kPi / static_cast<float>(picture.width);
  return across * (kPi / static_cast<float>(picture.height)) * std::cos(latitude);
}

void add_pixel(SkyHarmonics& sum, Vec3 direction, Vec3 light, float solid_angle) {
  const std::array<float, kSkyHarmonics> basis = harmonic_basis(direction);
  for (int i = 0; i < kSkyHarmonics; ++i) sum[i] = sum[i] + light * (basis[i] * solid_angle);
}

}  // namespace

std::array<float, kSkyHarmonics> harmonic_basis(Vec3 d) {
  return {kBand0,
          kBand1 * d.y,
          kBand1 * d.z,
          kBand1 * d.x,
          kBand2Cross * d.x * d.y,
          kBand2Cross * d.y * d.z,
          kBand2Zonal * (kThree * d.z * d.z - 1.0f),
          kBand2Cross * d.x * d.z,
          kBand2Square * (d.x * d.x - d.y * d.y)};
}

SkyHarmonics sky_irradiance(const FloatImage& picture) {
  SkyHarmonics sum{};
  for (std::uint32_t y = 0; y < picture.height; ++y) {
    const float solid_angle = pixel_solid_angle(picture, y);
    for (std::uint32_t x = 0; x < picture.width; ++x) {
      const PictureSpot spot{(static_cast<float>(x) + kHalf) / static_cast<float>(picture.width),
                             (static_cast<float>(y) + kHalf) / static_cast<float>(picture.height)};
      add_pixel(sum, picture_direction(spot), picture.at(x, y), solid_angle);
    }
  }
  for (int i = 0; i < kSkyHarmonics; ++i) sum[i] = sum[i] * kLobes[i];
  return sum;
}

HarmonicFloats harmonic_floats(const SkyHarmonics& harmonics) {
  HarmonicFloats floats{};
  for (std::size_t i = 0; i < harmonics.size(); ++i) {
    floats[i * kChannelsPerPixel + kXOffset] = harmonics[i].x;
    floats[i * kChannelsPerPixel + kYOffset] = harmonics[i].y;
    floats[i * kChannelsPerPixel + kZOffset] = harmonics[i].z;
  }
  return floats;
}

Vec3 irradiance_toward(const SkyHarmonics& harmonics, Vec3 normal) {
  const std::array<float, kSkyHarmonics> basis = harmonic_basis(normalize(normal));
  Vec3 total;
  for (int i = 0; i < kSkyHarmonics; ++i) total = total + harmonics[i] * basis[i];
  return total;
}

}  // namespace redgem::render_math
