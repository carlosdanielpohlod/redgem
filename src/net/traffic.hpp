#pragma once

#include <cstddef>
#include <cstdint>

namespace redgem::net {

// The bytes this program's message connections (Connection) received and sent since it started,
// from every thread at once: what a dedicated server's status turns into bytes a second.
struct TrafficTotals {
  std::uint64_t received = 0;
  std::uint64_t sent = 0;
};

// Adds to the totals (any thread).
void count_received(std::size_t bytes);
void count_sent(std::size_t bytes);
TrafficTotals traffic_totals();

}  // namespace redgem::net
