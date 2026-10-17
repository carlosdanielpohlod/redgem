#include "render/half_float.hpp"

#include <cmath>
#include <cstring>

namespace redgem::render_math {
namespace {

constexpr float kLargestHalf = 65504.0f;
constexpr float kSmallestNormalHalf = 6.103515625e-05f;  // 2^-14: below, it is taken as 0
constexpr int kFloatMantissaBits = 23;
constexpr int kHalfMantissaBits = 10;
constexpr int kDroppedBits = kFloatMantissaBits - kHalfMantissaBits;
constexpr std::uint32_t kExponentRebias = (127u - 15u) << kFloatMantissaBits;  // float's bias, half's bias
constexpr std::uint32_t kRoundingHalf = 1u << (kDroppedBits - 1);              // rounds to the nearest

}  // namespace

std::uint16_t to_half(float value) {
  if (!(value >= kSmallestNormalHalf)) return 0;  // (not-a-number fails every comparison)
  std::uint32_t bits = 0;
  const float held = std::fmin(value, kLargestHalf);
  std::memcpy(&bits, &held, sizeof(bits));
  return static_cast<std::uint16_t>((bits - kExponentRebias + kRoundingHalf) >> kDroppedBits);
}

void add_texel(std::vector<std::uint16_t>& texels, Vec3 light) {
  texels.insert(texels.end(), {to_half(light.x), to_half(light.y), to_half(light.z), to_half(1.0f)});
}

}  // namespace redgem::render_math
