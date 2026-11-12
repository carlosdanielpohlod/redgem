#include "net/traffic.hpp"

#include <atomic>

namespace redgem::net {

namespace {

// (Counted, not ordered: nothing else is read or written through them.)
std::atomic<std::uint64_t> received_bytes{0};
std::atomic<std::uint64_t> sent_bytes{0};

}  // namespace

void count_received(std::size_t bytes) { received_bytes.fetch_add(bytes, std::memory_order_relaxed); }

void count_sent(std::size_t bytes) { sent_bytes.fetch_add(bytes, std::memory_order_relaxed); }

TrafficTotals traffic_totals() {
  return {received_bytes.load(std::memory_order_relaxed), sent_bytes.load(std::memory_order_relaxed)};
}

}  // namespace redgem::net
