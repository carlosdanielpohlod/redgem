// The sky's cube map of reflections: the sharpest level is the picture as it is, each level below
// it the picture blurred for a rougher surface. A sample of the blur is read from the level of the
// picture's pyramid whose pixels are as large as the sample's share of the sphere, so that a few
// dozen samples are enough and bright spots do not turn into speckles.

#include <algorithm>
#include <cmath>

#include "render/half_float.hpp"
#include "render/sky_directions.hpp"
#include "render/sky_specular.hpp"

namespace redgem::render_math {
namespace {

constexpr std::uint32_t kBlurSamples = 64;
constexpr float kSphere = 4.0f * kPi;  // steradians
constexpr float kFinerLevel = 1.0f;    // a level sharper than the sample's size keeps detail
constexpr std::uint32_t kHalved = 2;   // each level of the pyramid half the one above

// Whether a level of the pyramid can still be halved.
bool halvable(const FloatImage& level) { return level.width > 1 && level.height > 1; }

float roughness_of(int level) {
  return std::min(static_cast<float>(level) / static_cast<float>(kSkyRoughestLevel), 1.0f);
}

// The level of the pyramid as coarse as a sample that was this likely.
const FloatImage& level_for(const std::vector<FloatImage>& pyramid, float likelihood) {
  const std::size_t pixels = static_cast<std::size_t>(pyramid[0].width) * pyramid[0].height;
  const float pixel = kSphere / static_cast<float>(std::max<std::size_t>(pixels, 1));
  const float sample = 1.0f / (static_cast<float>(kBlurSamples) * std::max(likelihood, kEpsilon));
  const float level = kHalf * std::log2(std::max(sample / pixel, 1.0f)) + kFinerLevel;
  const auto index = static_cast<std::size_t>(std::clamp(level, 0.0f, static_cast<float>(pyramid.size() - 1)));
  return pyramid[index];
}

// The sky around `direction` as a surface of `roughness` reflects it, seen head on.
Vec3 blurred(const std::vector<FloatImage>& pyramid, Vec3 direction, float roughness) {
  if (roughness <= 0.0f) return picture_light(pyramid[0], direction);
  Vec3 sum;
  float weight = 0.0f;
  for (std::uint32_t i = 0; i < kBlurSamples; ++i) {
    float first = 0.0f;
    float second = 0.0f;
    spread_pair(i, kBlurSamples, first, second);
    const SampledReflection sample = sample_reflection(direction, roughness, first, second);
    const float facing = dot(sample.direction, direction);
    if (facing <= 0.0f) continue;
    sum = sum + picture_light(level_for(pyramid, sample.likelihood), sample.direction) * facing;
    weight += facing;
  }
  return weight > 0.0f ? sum / weight : Vec3::zero();
}

void add_face(std::vector<std::uint16_t>& texels, const std::vector<FloatImage>& pyramid, int face, int level) {
  const int size = std::max(kSkyCubeSize >> level, 1);
  const float roughness = roughness_of(level);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float s = (static_cast<float>(x) + kHalf) / static_cast<float>(size);
      const float t = (static_cast<float>(y) + kHalf) / static_cast<float>(size);
      add_texel(texels, blurred(pyramid, cube_direction(face, s, t), roughness));
    }
  }
}

}  // namespace

std::vector<FloatImage> picture_pyramid(const FloatImage& picture) {
  std::vector<FloatImage> pyramid{picture};
  while (halvable(pyramid.back())) pyramid.push_back(shrunk(pyramid.back(), kHalved));
  return pyramid;
}

std::vector<std::uint16_t> sky_reflection_cube(const std::vector<FloatImage>& pyramid) {
  std::vector<std::uint16_t> texels;
  for (int level = 0; level < kSkyCubeLevels; ++level) {
    for (int face = 0; face < kCubeMapFaces; ++face) add_face(texels, pyramid, face, level);
  }
  return texels;
}

}  // namespace redgem::render_math
