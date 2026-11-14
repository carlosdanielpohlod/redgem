#include "core/frame_budget.hpp"

#include <utility>

namespace redgem {

FrameBudget::FrameBudget(Stopwatch::Reading clock, double seconds)
    : clock_(clock ? std::move(clock) : Stopwatch::Reading(steady_seconds)), seconds_(seconds), started_(clock_()) {}

bool FrameBudget::spent() const { return used() >= seconds_; }

double FrameBudget::used() const { return clock_() - started_; }

}  // namespace redgem
