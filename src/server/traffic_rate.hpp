#pragma once

#include <deque>

#include "net/traffic.hpp"

// The bytes a second a dedicated server receives and sends, over the last few seconds: what its
// status says (see server_status.hpp).
namespace redgem::server {

// A sample is taken at most once a second, and the rate is over the last ten seconds of them.
inline constexpr double kTrafficSampleSeconds = 1.0;
inline constexpr std::size_t kTrafficSamples = 11;

struct BytesPerSecond {
  double received = 0.0;
  double sent = 0.0;
};

class TrafficRate {
 public:
  // The totals at `now` (seconds on a clock that only goes forward): kept when a sample is due.
  void note(double now, const net::TrafficTotals& totals);
  // From the oldest sample kept to the newest; 0 until there are two.
  BytesPerSecond per_second() const;

 private:
  struct Sample {
    double at = 0.0;
    net::TrafficTotals totals;
  };

  bool sample_due(double now) const;

  std::deque<Sample> samples_;  // oldest first
};

}  // namespace redgem::server
