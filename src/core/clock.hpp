#pragma once

namespace redgem {

// Where code that keeps to real time reads it, and how it sleeps: the computer's steady clock in
// the engine (steady_clock()), a clock a test moves by hand in tests (tests/fake_clock.hpp). What
// takes a clock uses the steady one unless given another. Safe to use from any thread.
class Clock {
 public:
  Clock() = default;
  virtual ~Clock() = default;
  Clock(const Clock&) = delete;
  Clock& operator=(const Clock&) = delete;

  // Seconds from some fixed moment; never goes back.
  virtual double now() const = 0;
  // Sleeps `seconds`; not at all when that is no time (0 or less).
  virtual void sleep(double seconds) = 0;
};

// The computer's steady clock: real time, which nothing a program does speeds up or holds back.
// One for the whole program.
Clock& steady_clock();

}  // namespace redgem
