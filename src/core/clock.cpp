#include "core/clock.hpp"

#include <chrono>
#include <thread>

#include "core/stopwatch.hpp"

namespace redgem {

namespace {

class SteadyClock final : public Clock {
 public:
  double now() const override { return steady_seconds(); }

  void sleep(double seconds) override {
    if (seconds > 0.0) std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
  }
};

}  // namespace

Clock& steady_clock() {
  static SteadyClock clock;
  return clock;
}

}  // namespace redgem
