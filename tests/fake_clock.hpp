#pragma once

#include <mutex>
#include <thread>

#include "core/clock.hpp"

namespace redgem::test {

inline constexpr double kFakeClockStart = 1000.0;  // seconds: a clock reads from any moment

// A clock that only moves when a test moves it (advance) or when what uses it sleeps: a sleep
// takes no real time, it moves the clock on by what was asked, and by `oversleep` more (a busy
// machine wakes late). Shared by threads.
class FakeClock final : public Clock {
 public:
  explicit FakeClock(double oversleep = 0.0) : oversleep_(oversleep) {}

  double now() const override {
    const std::lock_guard<std::mutex> hold(lock_);
    return now_;
  }

  void sleep(double seconds) override {
    if (seconds > 0.0) note_sleep(seconds);
    std::this_thread::yield();  // (what spins on it lets the others go on)
  }

  void advance(double seconds) {
    const std::lock_guard<std::mutex> hold(lock_);
    now_ += seconds;
  }

  // All the seconds sleeps asked for, and how many sleeps there were.
  double slept() const {
    const std::lock_guard<std::mutex> hold(lock_);
    return slept_;
  }
  int sleeps() const {
    const std::lock_guard<std::mutex> hold(lock_);
    return sleeps_;
  }

 private:
  void note_sleep(double seconds) {
    const std::lock_guard<std::mutex> hold(lock_);
    now_ += seconds + oversleep_;
    slept_ += seconds;
    ++sleeps_;
  }

  const double oversleep_;
  mutable std::mutex lock_;  // guards the three below
  double now_ = kFakeClockStart;
  double slept_ = 0.0;
  int sleeps_ = 0;
};

}  // namespace redgem::test
