#include <doctest/doctest.h>

#include "core/fixed_clock.hpp"

using redgem::FixedClock;

namespace {
constexpr float kStep = 0.1f;
constexpr float kMaxFrame = 1.0f;
constexpr int kMaxSteps = 3;
}  // namespace

TEST_CASE("fixed clock accumulates partial frames") {
  FixedClock clock(kStep, kMaxFrame, kMaxSteps);
  CHECK(clock.advance(0.05f) == 0);
  CHECK(clock.advance(0.06f) == 1);
  CHECK(clock.alpha() == doctest::Approx(0.1f).epsilon(0.01));
}

TEST_CASE("fixed clock caps steps per frame and drops the backlog") {
  FixedClock clock(kStep, kMaxFrame, kMaxSteps);
  CHECK(clock.advance(0.95f) == kMaxSteps);
  CHECK(clock.advance(0.0f) <= 1);
}

TEST_CASE("fixed clock clamps huge and negative frame times") {
  FixedClock clock(kStep, kMaxFrame, kMaxSteps);
  CHECK(clock.advance(-5.0f) == 0);
  CHECK(clock.advance(100.0f) == kMaxSteps);
}
