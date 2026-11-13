#include "server/traffic_rate.hpp"

namespace redgem::server {

namespace {

constexpr std::size_t kSamplesForARate = 2;

double rate(std::uint64_t from, std::uint64_t to, double seconds) { return static_cast<double>(to - from) / seconds; }

}  // namespace

bool TrafficRate::sample_due(double now) const {
  return samples_.empty() || now - samples_.back().at >= kTrafficSampleSeconds;
}

void TrafficRate::note(double now, const net::TrafficTotals& totals) {
  if (!sample_due(now)) return;
  samples_.push_back({now, totals});
  if (samples_.size() > kTrafficSamples) samples_.pop_front();
}

BytesPerSecond TrafficRate::per_second() const {
  if (samples_.size() < kSamplesForARate) return {};
  const Sample& oldest = samples_.front();
  const Sample& newest = samples_.back();
  const double seconds = newest.at - oldest.at;
  return {rate(oldest.totals.received, newest.totals.received, seconds),
          rate(oldest.totals.sent, newest.totals.sent, seconds)};
}

}  // namespace redgem::server
