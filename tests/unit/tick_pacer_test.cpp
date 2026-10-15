#include <doctest/doctest.h>

#include "server/tick_pacer.hpp"

using namespace redgem::server;

namespace {

constexpr double kTick = 0.25;
constexpr double kStart = 100.0;
constexpr double kTolerance = 1e-6;

}  // namespace

TEST_CASE("a server's turn is due one tick after the one before") {
  TickPacer pacer(kTick, kStart);
  CHECK(pacer.until_next(kStart) == doctest::Approx(kTick).epsilon(kTolerance));
  CHECK(pacer.until_next(kStart + kTick / 2.0) == doctest::Approx(kTick / 2.0).epsilon(kTolerance));
  CHECK(pacer.until_next(kStart + kTick) == 0.0);
  CHECK(pacer.until_next(kStart + 3.0 * kTick) == 0.0);
}

TEST_CASE("each turn advances the game by the real time since the one before") {
  TickPacer pacer(kTick, kStart);
  CHECK(pacer.take_turn(kStart + kTick) == doctest::Approx(kTick));
  // A late turn covers all the time it was late by, once: the next is due a tick after it.
  CHECK(pacer.take_turn(kStart + 4.0 * kTick) == doctest::Approx(3.0 * kTick));
  CHECK(pacer.until_next(kStart + 4.0 * kTick) == doctest::Approx(kTick).epsilon(kTolerance));
}
