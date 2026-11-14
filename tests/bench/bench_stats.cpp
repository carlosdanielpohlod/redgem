#include "bench/bench_stats.hpp"

#include <algorithm>
#include <cmath>

namespace redgem::bench {

double percentile(const std::vector<double>& sorted, double share) {
  if (sorted.empty()) return 0.0;
  const double rank = share * static_cast<double>(sorted.size() - 1);
  const auto below = static_cast<std::size_t>(std::floor(rank));
  const std::size_t above = std::min(below + 1, sorted.size() - 1);
  const double weight = rank - static_cast<double>(below);
  return sorted[below] + (sorted[above] - sorted[below]) * weight;
}

Summary summarize(std::vector<double> samples) {
  Summary summary;
  if (samples.empty()) return summary;
  std::sort(samples.begin(), samples.end());
  summary.samples = samples.size();
  summary.median = percentile(samples, kMedianShare);
  summary.p95 = percentile(samples, kP95Share);
  summary.min = samples.front();
  summary.max = samples.back();
  return summary;
}

}  // namespace redgem::bench
