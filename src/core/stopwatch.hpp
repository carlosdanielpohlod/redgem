#pragma once

#include <functional>

namespace redgem {

// Seconds on the computer's steady clock, from some fixed moment: real time, which nothing a
// program does speeds up or holds back.
double steady_seconds();

// Counts the real time that goes by between laps, however long the game's frames say they took (a
// game with nobody to show it runs its frames as fast as it can).
class Stopwatch {
 public:
  // Where it reads the time: seconds from any fixed moment.
  using Reading = std::function<double()>;

  // Reads the computer's steady clock when given no reading of its own.
  explicit Stopwatch(Reading reading = {});
  // Seconds since the last lap, or since it was made for the first one.
  float lap();
  // Starts the next lap now: what went by since the last one (a game starting, a scene loading)
  // does not count.
  void restart();

 private:
  Reading reading_;
  double last_ = 0.0;
};

}  // namespace redgem
