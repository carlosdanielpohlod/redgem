#pragma once

#include <cstdint>

namespace redgem {

// Milliseconds on a clock that only goes forward, read as cheaply as the system allows: a few
// nanoseconds, at the price of a resolution of a few milliseconds (the system's tick: 1 to 16 ms).
// For what is read very often and measured in tens of milliseconds or more, such as how long a
// call into the scripts has run.
std::int64_t coarse_milliseconds();

// The time `span` milliseconds after `now` on that clock; the latest time it can hold when that is
// further than it can count (a span of "never", say).
std::int64_t milliseconds_after(std::int64_t now, std::int64_t span);

}  // namespace redgem
