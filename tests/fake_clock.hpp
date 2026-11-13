#pragma once

#include <chrono>
#include <mutex>
#include <thread>

#include "core/clock.hpp"

namespace redgem::test {

inline constexpr double kFakeClockStart = 1000.0;  // seconds: a clock reads from any moment

// Whether a sleep on a fake clock also takes real time: for a thread that would otherwise spin.
enum class RealSleep { No, Yes };

// A clock that only moves when a test moves it (advance) or when what uses it sleeps: a sleep
// moves it on by what was asked, and by `oversleep` more (a busy machine wakes late). Unless told
// to, a sleep takes no real time. Shared by threads.
class FakeClock final : public Clock {
 public:
  explicit FakeClock(double oversleep = 0.0, RealSleep real = RealSleep::No) : oversleep_(oversleep), real_(real) {}

  double now() const override {
    const std::lock_guard<std::mutex> hold(lock_);
    return now_;
  }

  void sleep(double seconds) override {
    if (seconds > 0.0) note_sleep(seconds);
    if (sleeps_really(seconds)) std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
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
  bool sleeps_really(double seconds) const { return seconds > 0.0 && real_ == RealSleep::Yes; }

  void note_sleep(double seconds) {
    const std::lock_guard<std::mutex> hold(lock_);
    now_ += seconds + oversleep_;
    slept_ += seconds;
    ++sleeps_;
  }

  const double oversleep_;
  const RealSleep real_;
  mutable std::mutex lock_;  // guards the three below
  double now_ = kFakeClockStart;
  double slept_ = 0.0;
  int sleeps_ = 0;
};

}  // namespace redgem::test
