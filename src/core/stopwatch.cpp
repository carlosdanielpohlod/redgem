#include "core/stopwatch.hpp"

#include <chrono>
#include <utility>

namespace redgem {

double steady_seconds() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Stopwatch::Stopwatch(Reading reading) : reading_(reading ? std::move(reading) : Reading(steady_seconds)) {
  last_ = reading_();
}

float Stopwatch::lap() {
  const double now = reading_();
  const double elapsed = now - last_;
  last_ = now;
  return static_cast<float>(elapsed);
}

void Stopwatch::restart() { last_ = reading_(); }

}  // namespace redgem
