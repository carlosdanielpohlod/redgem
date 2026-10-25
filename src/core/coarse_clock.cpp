#include "core/coarse_clock.hpp"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <time.h>
#else
#include <ctime>
#endif

#include <limits>

namespace redgem {
namespace {

#if !defined(_WIN32)
constexpr std::int64_t kNanosecondsPerMillisecond = 1000 * 1000;
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
constexpr std::int64_t kMillisecondsPerSecond = 1000;
#endif

constexpr std::int64_t kLatest = std::numeric_limits<std::int64_t>::max();

}  // namespace

std::int64_t milliseconds_after(std::int64_t now, std::int64_t span) {
  if (span > kLatest - now) return kLatest;
  return now + span;
}

#if defined(_WIN32)
// (The system's tick count: no call into the kernel.)
std::int64_t coarse_milliseconds() { return static_cast<std::int64_t>(GetTickCount64()); }
#elif defined(__APPLE__)
std::int64_t coarse_milliseconds() {
  return static_cast<std::int64_t>(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW_APPROX)) / kNanosecondsPerMillisecond;
}
#else
// (The kernel's last tick, read without a call into it.)
std::int64_t coarse_milliseconds() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
  return static_cast<std::int64_t>(now.tv_sec) * kMillisecondsPerSecond + now.tv_nsec / kNanosecondsPerMillisecond;
}
#endif

}  // namespace redgem
