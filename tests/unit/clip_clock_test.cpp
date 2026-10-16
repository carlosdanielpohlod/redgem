#include <doctest/doctest.h>

#include "scene/animation/clip_clock.hpp"

using namespace redgem;

namespace {

constexpr float kDuration = 2.0f;  // seconds the clip lasts
constexpr float kStep = 0.5f;
constexpr float kBackwards = -1.0f;
constexpr float kDouble = 2.0f;

ClipClock played_once() {
  ClipClock clock;
  start(clock, kDuration, false, 1.0f, true);
  return clock;
}

}  // namespace

TEST_CASE("a clock moves on by its speed, and not at all while it is not playing") {
  ClipClock clock;
  clock.speed = kDouble;
  advance(clock, kDuration, kStep);
  CHECK(clock.time == doctest::Approx(kStep * kDouble));

  clock.playing = false;
  advance(clock, kDuration, kStep);
  CHECK(clock.time == doctest::Approx(kStep * kDouble));
}

TEST_CASE("a looping clock wraps around either end of its clip") {
  ClipClock clock;
  advance(clock, kDuration, kDuration + kStep);
  CHECK(clock.time == doctest::Approx(kStep));
  CHECK(clock.playing);

  ClipClock reversed;
  reversed.speed = kBackwards;
  advance(reversed, kDuration, kStep);
  CHECK(reversed.time == doctest::Approx(kDuration - kStep));
  CHECK(reversed.playing);
}

TEST_CASE("a clock played once stops at the end it reaches") {
  ClipClock forwards = played_once();
  advance(forwards, kDuration, kDuration + kStep);
  CHECK(forwards.time == doctest::Approx(kDuration));
  CHECK_FALSE(forwards.playing);

  ClipClock reversed;
  start(reversed, kDuration, false, kBackwards, true);
  CHECK(reversed.time == doctest::Approx(kDuration));  // played backwards, it starts from its end
  advance(reversed, kDuration, kDuration + kStep);
  CHECK(reversed.time == doctest::Approx(0.0f));
  CHECK_FALSE(reversed.playing);
}

TEST_CASE("starting a clock carries on from where it is, unless it has nowhere to go") {
  ClipClock clock;
  advance(clock, kDuration, kStep);
  clock.playing = false;
  start(clock, kDuration, true, 1.0f, false);
  CHECK(clock.playing);
  CHECK(clock.time == doctest::Approx(kStep));  // paused: it carries on

  start(clock, kDuration, true, 1.0f, true);
  CHECK(clock.time == doctest::Approx(0.0f));  // asked to start over

  ClipClock ended = played_once();
  advance(ended, kDuration, kDuration);
  start(ended, kDuration, false, 1.0f, false);
  CHECK(ended.time == doctest::Approx(0.0f));  // played once and ended: it starts over
  CHECK(ended.playing);

  ClipClock stopped;
  advance(stopped, kDuration, kStep);
  stopped.posing = false;
  start(stopped, kDuration, true, 1.0f, false);
  CHECK(stopped.time == doctest::Approx(0.0f));  // stopped: it starts over, and poses again
  CHECK(stopped.posing);
}

TEST_CASE("a clip without a length keeps a looping clock's time as it is") {
  ClipClock clock;
  advance(clock, 0.0f, kStep);
  CHECK(clock.time == doctest::Approx(kStep));
  CHECK(clock.playing);
}
