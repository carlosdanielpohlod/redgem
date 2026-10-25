// Times on the coarse clock: a span after now, without wrapping around past what it can hold.
#include <doctest/doctest.h>

#include <cstdint>
#include <limits>

#include "core/coarse_clock.hpp"

using redgem::milliseconds_after;

namespace {
constexpr std::int64_t kLatest = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kNow = 1879280;  // a time the watchdog of the scripts once added "never" to
constexpr std::int64_t kSecond = 1000;
}  // namespace

TEST_CASE("a span after now is their sum") { CHECK(milliseconds_after(kNow, kSecond) == kNow + kSecond); }

TEST_CASE("a span further than the clock can count is the latest time, not one that wrapped around") {
  // (Found by UndefinedBehaviorSanitizer: the watchdog of the scripts added "never" to now.)
  CHECK(milliseconds_after(kNow, kLatest) == kLatest);
  CHECK(milliseconds_after(kLatest, 1) == kLatest);
}
