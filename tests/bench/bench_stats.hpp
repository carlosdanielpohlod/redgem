#pragma once

#include <cstddef>
#include <vector>

// What repeated measurements of one thing come to: the numbers a benchmark reports and the
// thresholds (tests/bench/thresholds.json) are checked against.
namespace redgem::bench {

inline constexpr double kMedianShare = 0.5;
inline constexpr double kP95Share = 0.95;

struct Summary {
  std::size_t samples = 0;
  double median = 0.0;
  double p95 = 0.0;
  double min = 0.0;
  double max = 0.0;
};

// The value `share` (0 to 1) of the way through sorted `values`, between the two nearest ranks;
// 0 for no values.
double percentile(const std::vector<double>& sorted, double share);
// The median, 95th percentile, smallest and largest of `samples` (all 0 for none).
Summary summarize(std::vector<double> samples);

}  // namespace redgem::bench
