// Reflections taken at random the GGX way, and the table of how much of the blurred sky a surface
// reflects.

#include <cmath>

#include "render/half_float.hpp"
#include "render/sky_specular.hpp"

namespace redgem::render_math {
namespace {

constexpr std::uint32_t kReflectanceSamples = 256;
constexpr float kNearlyUp = 0.999f;  // a normal this close to the vertical takes another axis
constexpr float kFresnelPower = 5.0f;
constexpr float kBitsToUnit = 2.3283064365386963e-10f;  // 2^-32
constexpr int kHalfWord = 16;
constexpr int kQuarterWord = 8;
constexpr int kEighthWord = 4;
constexpr int kTwoBits = 2;
constexpr std::uint32_t kHalves = 0x55555555u;  // masks that swap the bits of a word around
constexpr std::uint32_t kQuarters = 0x33333333u;
constexpr std::uint32_t kEighths = 0x0F0F0F0Fu;
constexpr std::uint32_t kBytes = 0x00FF00FFu;

std::uint32_t reversed_bits(std::uint32_t bits) {
  bits = (bits << kHalfWord) | (bits >> kHalfWord);
  bits = ((bits & kBytes) << kQuarterWord) | ((bits & ~kBytes) >> kQuarterWord);
  bits = ((bits & kEighths) << kEighthWord) | ((bits & ~kEighths) >> kEighthWord);
  bits = ((bits & kQuarters) << kTwoBits) | ((bits & ~kQuarters) >> kTwoBits);
  return ((bits & kHalves) << 1) | ((bits & ~kHalves) >> 1);
}

// GGX's distribution of normals, for the cosine between the surface's and the half vector.
float distribution(float cos_half, float alpha) {
  const float alpha2 = alpha * alpha;
  const float d = cos_half * cos_half * (alpha2 - 1.0f) + 1.0f;
  return alpha2 / (kPi * d * d);
}

// Smith's shadowing of one direction, with k for lighting from all around (alpha / 2).
float shadowing(float cos_angle, float k) { return cos_angle / (cos_angle * (1.0f - k) + k); }

// The half vector GGX picks for the two numbers, around `normal`.
Vec3 half_vector(Vec3 normal, float alpha, float first, float second) {
  const float turn = kTwo * kPi * first;
  const float cos_theta = std::sqrt((1.0f - second) / (1.0f + (alpha * alpha - 1.0f) * second));
  const float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);
  const Vec3 side = std::fabs(normal.y) < kNearlyUp ? Vec3::up() : Vec3::right();
  const Vec3 tangent = normalize(cross(side, normal));
  const Vec3 bitangent = cross(normal, tangent);
  return normalize(tangent * (std::cos(turn) * sin_theta) + bitangent * (std::sin(turn) * sin_theta) +
                   normal * cos_theta);
}

// One sample of the table's entry for a view (around +Z): {scale, bias, 0}.
Vec3 reflected_share(Vec3 view, float alpha, float first, float second) {
  const Vec3 h = half_vector(Vec3{0.0f, 0.0f, 1.0f}, alpha, first, second);
  const float view_half = std::fmax(dot(view, h), 0.0f);
  const Vec3 light = h * (kTwo * view_half) - view;
  if (light.z <= 0.0f) return Vec3::zero();
  const float k = alpha * kHalf;
  const float visible = shadowing(view.z, k) * shadowing(light.z, k) * view_half / (h.z * view.z);
  const float fresnel = std::pow(1.0f - view_half, kFresnelPower);
  return {(1.0f - fresnel) * visible, fresnel * visible, 0.0f};
}

}  // namespace

void spread_pair(std::uint32_t i, std::uint32_t count, float& first, float& second) {
  first = static_cast<float>(i) / static_cast<float>(count);
  second = static_cast<float>(reversed_bits(i)) * kBitsToUnit;
}

SampledReflection sample_reflection(Vec3 normal, float roughness, float first, float second) {
  const float alpha = roughness * roughness;
  const Vec3 h = half_vector(normal, alpha, first, second);
  const float cos_half = dot(normal, h);
  // Seen along the normal, the half vector's angle to the view is its angle to the normal.
  return {h * (kTwo * cos_half) - normal, distribution(cos_half, alpha) / (kTwo * kTwo)};
}

Vec3 reflectance(float cos_view, float roughness) {
  const Vec3 view{std::sqrt(1.0f - cos_view * cos_view), 0.0f, cos_view};
  Vec3 sum;
  for (std::uint32_t i = 0; i < kReflectanceSamples; ++i) {
    float first = 0.0f;
    float second = 0.0f;
    spread_pair(i, kReflectanceSamples, first, second);
    sum = sum + reflected_share(view, roughness * roughness, first, second);
  }
  return sum / static_cast<float>(kReflectanceSamples);
}

std::vector<std::uint16_t> reflectance_table() {
  std::vector<std::uint16_t> texels;
  const auto side = static_cast<float>(kReflectanceTableSize);
  for (int y = 0; y < kReflectanceTableSize; ++y) {
    for (int x = 0; x < kReflectanceTableSize; ++x) {
      add_texel(texels, reflectance((static_cast<float>(x) + kHalf) / side, (static_cast<float>(y) + kHalf) / side));
    }
  }
  return texels;
}

}  // namespace redgem::render_math
