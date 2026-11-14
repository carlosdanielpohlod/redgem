#include "server/tick_pacer.hpp"

#include <algorithm>

namespace redgem::server {

double TickPacer::until_next(double now) const { return std::max(0.0, last_turn_ + tick_seconds_ - now); }

float TickPacer::take_turn(double now) {
  const double elapsed = now - last_turn_;
  last_turn_ = now;
  return static_cast<float>(elapsed);
}

}  // namespace redgem::server
