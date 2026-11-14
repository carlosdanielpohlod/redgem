// What the benchmarks report of their samples (tests/bench/bench_stats.hpp): the median and 95th
// percentile the thresholds are checked against.

#include <doctest/doctest.h>

#include <vector>

#include "bench/bench_stats.hpp"

using redgem::bench::percentile;
using redgem::bench::summarize;
using redgem::bench::Summary;

TEST_CASE("bench stats: no samples summarize to nothing") {
  const Summary summary = summarize({});
  CHECK(summary.samples == 0);
  CHECK(summary.median == 0.0);
  CHECK(summary.p95 == 0.0);
}

TEST_CASE("bench stats: one sample is its own median and p95") {
  const Summary summary = summarize({4.0});
  CHECK(summary.median == 4.0);
  CHECK(summary.p95 == 4.0);
  CHECK(summary.min == 4.0);
  CHECK(summary.max == 4.0);
}

TEST_CASE("bench stats: samples are sorted before they are read") {
  const Summary summary = summarize({5.0, 1.0, 3.0, 2.0, 4.0});
  CHECK(summary.samples == 5);
  CHECK(summary.median == 3.0);
  CHECK(summary.min == 1.0);
  CHECK(summary.max == 5.0);
}

TEST_CASE("bench stats: a percentile falls between the two nearest ranks") {
  const std::vector<double> sorted = {10.0, 20.0};
  CHECK(percentile(sorted, 0.5) == doctest::Approx(15.0));
  CHECK(percentile(sorted, 0.95) == doctest::Approx(19.5));
}

TEST_CASE("bench stats: one slow sample in twenty moves the p95, not the median") {
  std::vector<double> samples(19, 1.0);
  samples.push_back(100.0);
  const Summary summary = summarize(samples);
  CHECK(summary.median == 1.0);
  CHECK(summary.p95 == doctest::Approx(1.0 + (100.0 - 1.0) * 0.05));
}
