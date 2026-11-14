#pragma once

#include <algorithm>

namespace redgem::net::stun {

// The longest a single update counts as: a game that hitched (a level loading, the first frame)
// does not use up a probe's time, or a route's, before it had a chance to hear back.
inline constexpr float kLongestCountedStepSeconds = 0.25f;

// The time an update of `seconds` counts as.
inline float counted_step(float seconds) { return std::min(seconds, kLongestCountedStepSeconds); }

}  // namespace redgem::net::stun
