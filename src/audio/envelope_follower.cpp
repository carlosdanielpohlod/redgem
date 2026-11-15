#include "audio/envelope_follower.hpp"

#include <cmath>

namespace redgem {
namespace {

constexpr float kWholeWay = 1.0f;

// The share of the way to the target that `seconds` take with a time constant of `tau` seconds.
float share_of_way(float seconds, float tau) {
  if (tau <= 0.0f) return kWholeWay;
  return kWholeWay - std::exp(-seconds / tau);
}

}  // namespace

float rms_level(const float* samples, std::size_t count) {
  if (count == 0) return kSilence;
  double sum = 0.0;
  for (std::size_t index = 0; index < count; ++index) sum += static_cast<double>(samples[index]) * samples[index];
  return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

float EnvelopeFollower::update(float level, float seconds) {
  const float tau = level > value_ ? attack_ : release_;
  value_ += (level - value_) * share_of_way(seconds, tau);
  return value_;
}

}  // namespace redgem
