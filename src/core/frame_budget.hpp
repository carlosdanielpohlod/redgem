#pragma once

#include "core/stopwatch.hpp"

namespace redgem {

// A share of one frame's time that some work may take, counted on a clock of seconds: work that
// is spread over frames asks it, between two of its pieces, whether the frame's share is used up.
class FrameBudget {
 public:
  // Starts now; `clock` empty reads the computer's steady clock.
  FrameBudget(Stopwatch::Reading clock, double seconds);
  // Whether the time it gives is up.
  bool spent() const;
  // Seconds since it started.
  double used() const;

 private:
  Stopwatch::Reading clock_;
  double seconds_ = 0.0;
  double started_ = 0.0;
};

}  // namespace redgem
