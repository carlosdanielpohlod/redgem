#pragma once

namespace redgem {

// Converts variable frame times into a whole number of fixed simulation steps.
class FixedClock {
 public:
  FixedClock(float fixed_delta, float max_frame_seconds, int max_steps_per_frame);

  // Feeds the elapsed real time and returns how many fixed steps to simulate now.
  int advance(float frame_seconds);

  float fixed_delta() const { return fixed_delta_; }
  // Fraction of a step left in the accumulator, useful for interpolation.
  float alpha() const { return accumulator_ / fixed_delta_; }

 private:
  // Whether one more step is to be taken this frame, with `taken` of them already.
  bool step_due(int taken) const;

  float fixed_delta_;
  float max_frame_seconds_;
  int max_steps_per_frame_;
  float accumulator_ = 0.0f;
};

}  // namespace redgem
