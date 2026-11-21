#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>

namespace redgem {

// The longest a wait for another thread (a scene read, a web answer, an agent's message) lasts.
inline constexpr float kLongestThreadWaitSeconds = 3600.0f;

// A wait of `seconds` as a condition variable takes it: whole milliseconds, never negative, never
// more than kLongestThreadWaitSeconds, and none for a length that is no number.
inline std::chrono::milliseconds wait_length(float seconds) {
  const float bounded = std::isnan(seconds) ? 0.0f : std::clamp(seconds, 0.0f, kLongestThreadWaitSeconds);
  return std::chrono::milliseconds(static_cast<long long>(bounded * std::milli::den));
}

}  // namespace redgem
