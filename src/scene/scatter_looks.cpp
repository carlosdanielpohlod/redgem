#include "scene/scatter_looks.hpp"

#include <algorithm>
#include <cmath>

#include "scene/dice.hpp"

namespace redgem {
namespace {

// The looks' dice start apart from the places' (the same seed must not make them follow each other).
constexpr std::uint64_t kLooksSeed = 0x6C6F6F6B00000000ULL;  // "look", above the 32 bits of a seed

// The color at `along` (0..1) of the way through the list.
Vec3 along_tints(const std::vector<Vec3>& tints, float along) {
  if (tints.size() == 1) return tints.front();
  const float place = along * static_cast<float>(tints.size() - 1);
  const auto first = std::min(static_cast<std::size_t>(place), tints.size() - 2);
  return mix(tints[first], tints[first + 1], place - static_cast<float>(first));
}

Vec3 one_tint(Dice& dice, const ScatterLooks& looks) {
  const Vec3 color = looks.tints.empty() ? Vec3::one() : along_tints(looks.tints, dice.next());
  return color * dice.between(1.0f - looks.tint_variation, 1.0f + looks.tint_variation);
}

}  // namespace

bool varied(const ScatterLooks& looks) { return !looks.tints.empty() || looks.tint_variation > 0.0f; }

std::vector<Vec3> scatter_tints(const ScatterLooks& looks, std::uint32_t seed, std::size_t count) {
  std::vector<Vec3> tints;
  if (!varied(looks)) return tints;
  Dice dice(kLooksSeed ^ seed);
  tints.reserve(count);
  for (std::size_t i = 0; i < count; ++i) tints.push_back(one_tint(dice, looks));
  return tints;
}

}  // namespace redgem
