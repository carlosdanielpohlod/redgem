#include "core/fixed_clock.hpp"

#include <algorithm>

namespace redgem {

FixedClock::FixedClock(float fixed_delta, float max_frame_seconds, int max_steps_per_frame)
    : fixed_delta_(fixed_delta), max_frame_seconds_(max_frame_seconds), max_steps_per_frame_(max_steps_per_frame) {}

bool FixedClock::step_due(int taken) const { return accumulator_ >= fixed_delta_ && taken < max_steps_per_frame_; }

int FixedClock::advance(float frame_seconds) {
  accumulator_ += std::clamp(frame_seconds, 0.0f, max_frame_seconds_);
  int steps = 0;
  while (step_due(steps)) {
    accumulator_ -= fixed_delta_;
    ++steps;
  }
  // Drop the backlog we refused to simulate instead of spiralling.
  if (steps == max_steps_per_frame_) accumulator_ = std::min(accumulator_, fixed_delta_);
  return steps;
}

}  // namespace redgem
