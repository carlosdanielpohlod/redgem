#pragma once

// Keeps a server's turns on the real clock: one every tick, each advancing the game by the real
// time since the turn before. A late turn is not made up for with a burst of quick ones: the next
// simply covers more time (and the engine steps its physics in fixed steps however long that is).
// Times are seconds on a clock that only goes forward.
namespace redgem::server {

// Seconds on a clock that only goes forward.
double steady_seconds();

class TickPacer {
 public:
  TickPacer(double tick_seconds, double now) : tick_seconds_(tick_seconds), last_turn_(now) {}

  // Seconds from `now` until the next turn is due; 0 when it is due already.
  double until_next(double now) const;
  // Takes a turn at `now`: the seconds of game it advances.
  float take_turn(double now);

 private:
  double tick_seconds_;
  double last_turn_;
};

}  // namespace redgem::server
