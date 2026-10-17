#include <doctest/doctest.h>

#include "core/stopwatch.hpp"

using redgem::Stopwatch;

namespace {
constexpr double kStart = 100.0;  // seconds: a clock reads from any moment
constexpr double kFirstLap = 2.5;
constexpr double kSecondLap = 0.25;
}  // namespace

TEST_CASE("a stopwatch's laps are the time its reading moved on since the one before") {
  double now = kStart;
  Stopwatch watch([&now] { return now; });
  now += kFirstLap;
  CHECK(watch.lap() == doctest::Approx(kFirstLap));
  now += kSecondLap;
  CHECK(watch.lap() == doctest::Approx(kSecondLap));
  CHECK(watch.lap() == doctest::Approx(0.0));  // nothing went by
}

TEST_CASE("a stopwatch given no reading of its own goes by the real clock") {
  Stopwatch watch;
  const double before = redgem::steady_seconds();
  const float lap = watch.lap();
  CHECK(lap >= 0.0f);
  CHECK(redgem::steady_seconds() >= before);
}
