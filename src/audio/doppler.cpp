#include "audio/doppler.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr float kFastestShare = 0.5f;             // of the speed of sound, either way
constexpr float kNearestDopplerDistance = 0.01f;  // m: nearer, the line between them has no direction

float bounded_speed(float speed) {
  constexpr float kFastest = kSpeedOfSound * kFastestShare;
  return std::clamp(speed, -kFastest, kFastest);
}

bool no_doppler(float scale, float distance) {
  if (!std::isfinite(scale) || scale <= kNoDoppler) return true;
  return !std::isfinite(distance) || distance < kNearestDopplerDistance;
}

}  // namespace

float doppler_shift(Vec3 listener, Vec3 listener_velocity, Vec3 source, Vec3 source_velocity, float scale) {
  const Vec3 offset = source - listener;
  const float distance = length(offset);
  if (no_doppler(scale, distance)) return kNoShift;
  const Vec3 toward = offset / distance;
  const float listener_speed = bounded_speed(dot(listener_velocity, toward) * scale);
  const float source_speed = bounded_speed(dot(source_velocity, toward) * scale);
  const float shift = (kSpeedOfSound + listener_speed) / (kSpeedOfSound + source_speed);
  return std::isfinite(shift) ? shift : kNoShift;
}

}  // namespace redgem
