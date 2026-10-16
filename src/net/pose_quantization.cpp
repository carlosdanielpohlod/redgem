#include "net/pose_quantization.hpp"

#include <algorithm>
#include <cmath>

namespace redgem::net {

namespace {

constexpr int kParts = 4;  // of a quaternion
constexpr int kPartBits = 10;
constexpr std::uint32_t kPartMask = (1U << kPartBits) - 1U;
constexpr float kPartSteps = static_cast<float>(kPartMask);
// Where the index of the largest part sits, and the first of the other three (the next ones
// follow, kPartBits lower each).
constexpr int kLargestShift = 3 * kPartBits;
constexpr int kFirstPartShift = 2 * kPartBits;
// The most any part but the largest can be, either way: 1/sqrt(2).
constexpr float kSmallRange = 0.70710678f;
constexpr float kMostSteps = 32767.0f;  // the longest move along an axis, in steps, that 16 bits hold

using Parts = std::array<float, kParts>;

Parts parts_of(Quat rotation) {
  Parts parts{};
  parts[kXOffset] = rotation.x;
  parts[kYOffset] = rotation.y;
  parts[kZOffset] = rotation.z;
  parts[kWOffset] = rotation.w;
  return parts;
}

int largest_part(const Parts& parts) {
  int largest = 0;
  for (int i = 1; i < kParts; ++i) {
    if (std::fabs(parts[i]) > std::fabs(parts[largest])) largest = i;
  }
  return largest;
}

std::uint32_t part_bits(float part) {
  const float share = std::clamp((part + kSmallRange) / (kTwo * kSmallRange), 0.0f, 1.0f);
  return static_cast<std::uint32_t>(std::lround(share * kPartSteps));
}

float part_from(std::uint32_t bits) { return static_cast<float>(bits) / kPartSteps * kTwo * kSmallRange - kSmallRange; }

bool fits(float steps) { return std::fabs(steps) <= kMostSteps; }

// Whether a move, in steps, fits in 16 bits along every axis (false for one that is not a number).
bool all_fit(Vec3 steps) { return fits(steps.x) && fits(steps.y) && fits(steps.z); }

std::int16_t rounded(float steps) { return static_cast<std::int16_t>(std::lround(steps)); }

}  // namespace

std::uint32_t packed_rotation(Quat rotation) {
  const Parts parts = parts_of(normalize(rotation));
  const int largest = largest_part(parts);
  // (q and -q are the same rotation: the one whose largest part is positive is sent.)
  const float sign = parts[largest] < 0.0f ? -1.0f : 1.0f;
  auto packed = static_cast<std::uint32_t>(largest) << kLargestShift;
  int shift = kFirstPartShift;
  for (int i = 0; i < kParts; ++i) {
    if (i == largest) continue;
    packed |= part_bits(parts[i] * sign) << shift;
    shift -= kPartBits;
  }
  return packed;
}

Quat unpacked_rotation(std::uint32_t packed) {
  const auto largest = static_cast<int>(packed >> kLargestShift);
  Parts parts{};
  float others = 0.0f;
  int shift = kFirstPartShift;
  for (int i = 0; i < kParts; ++i) {
    if (i == largest) continue;
    parts[i] = part_from((packed >> shift) & kPartMask);
    others += parts[i] * parts[i];
    shift -= kPartBits;
  }
  parts[largest] = std::sqrt(std::max(0.0f, 1.0f - others));
  return normalize(Quat{parts[kXOffset], parts[kYOffset], parts[kZOffset], parts[kWOffset]});
}

std::optional<PoseSteps> steps_between(Vec3 from, Vec3 to) {
  const Vec3 steps = (to - from) / kPoseStep;
  if (!all_fit(steps)) return std::nullopt;
  return PoseSteps{rounded(steps.x), rounded(steps.y), rounded(steps.z)};
}

Vec3 moved_by(Vec3 from, const PoseSteps& steps) {
  return {from.x + steps[kXOffset] * kPoseStep, from.y + steps[kYOffset] * kPoseStep,
          from.z + steps[kZOffset] * kPoseStep};
}

bool moves(const PoseSteps& steps) { return steps != PoseSteps{}; }

}  // namespace redgem::net
