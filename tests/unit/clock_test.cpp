#include <doctest/doctest.h>

#include "core/clock.hpp"
#include "fake_clock.hpp"

using redgem::test::FakeClock;
using redgem::test::kFakeClockStart;

namespace {
constexpr double kStep = 2.5;
constexpr double kNap = 0.25;
constexpr double kOversleep = 0.125;
}  // namespace

TEST_CASE("the steady clock never goes back, and a sleep of no time returns") {
  redgem::Clock& clock = redgem::steady_clock();
  const double before = clock.now();
  clock.sleep(0.0);
  clock.sleep(-1.0);  // (never "for ever")
  CHECK(clock.now() >= before);
  CHECK(&redgem::steady_clock() == &clock);  // one for the whole program
}

TEST_CASE("a fake clock moves only when moved, or slept on") {
  FakeClock clock;
  CHECK(clock.now() == kFakeClockStart);
  clock.advance(kStep);
  CHECK(clock.now() == doctest::Approx(kFakeClockStart + kStep));
  clock.sleep(kNap);
  CHECK(clock.now() == doctest::Approx(kFakeClockStart + kStep + kNap));
  clock.sleep(0.0);
  clock.sleep(-kNap);
  CHECK(clock.sleeps() == 1);
  CHECK(clock.slept() == doctest::Approx(kNap));
}

TEST_CASE("a fake clock that oversleeps wakes later than asked") {
  FakeClock clock(kOversleep);
  clock.sleep(kNap);
  CHECK(clock.now() == doctest::Approx(kFakeClockStart + kNap + kOversleep));
  CHECK(clock.slept() == doctest::Approx(kNap));  // what was asked
}
