#pragma once

#include <cstdint>

namespace redgem {

// Milliseconds on a clock that only goes forward, read as cheaply as the system allows: a few
// nanoseconds, at the price of a resolution of a few milliseconds (the system's tick: 1 to 16 ms).
// For what is read very often and measured in tens of milliseconds or more, such as how long a
// call into the scripts has run.
std::int64_t coarse_milliseconds();

}  // namespace redgem
