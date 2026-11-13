// The engine's benchmarks: the hot paths that need no window and no graphics card (scripts,
// physics, scene loading, the network frame, saves, HTTP answers), each timed over many samples.
// Not a test, not shipped: the CI's Benchmarks workflow runs it, and tools/bench/check_thresholds.py
// holds what it measured to tests/bench/thresholds.json.
//
//   redgem_bench [--runs 30] [--warmup 3] [--out bench.json] [--only <part of a name>]
//
// It prints a table (Markdown) of each benchmark's median and 95th percentile, in microseconds per
// operation, and writes them as JSON with --out. It fails when a benchmark could not run.

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include "bench/bench_case.hpp"
#include "bench/bench_results.hpp"
#include "bench/bench_stats.hpp"

namespace {

using redgem::bench::BenchCase;
using redgem::bench::Result;
using redgem::bench::Sampler;

constexpr int kDefaultRuns = 30;
constexpr int kDefaultWarmup = 3;
constexpr const char* kSuite = "bench";
constexpr const char* kMicroseconds = "us";

struct Options {
  int runs = kDefaultRuns;
  int warmup = kDefaultWarmup;
  std::string out;
  std::string only;
};

// Reads the value of an option that takes one into `options`; false when `arg` is none of them.
bool read_option(const std::string& arg, const std::string& value, Options& options) {
  if (arg == "--runs")
    options.runs = std::stoi(value);
  else if (arg == "--warmup")
    options.warmup = std::stoi(value);
  else if (arg == "--out")
    options.out = value;
  else if (arg == "--only")
    options.only = value;
  else
    return false;
  return true;
}

Options read_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const bool has_value = i + 1 < argc;
    if (!has_value || !read_option(argv[i], argv[i + 1], options))
      throw std::invalid_argument(std::string("unknown option or missing value: ") + argv[i]);
    ++i;
  }
  if (options.runs < 1) throw std::invalid_argument("--runs needs at least 1");
  return options;
}

Result run_case(const BenchCase& bench, const Options& options) {
  const Sampler sample = bench.prepare();
  for (int i = 0; i < options.warmup; ++i) sample();
  std::vector<double> samples;
  for (int i = 0; i < options.runs; ++i) samples.push_back(sample());
  return {bench.name, kMicroseconds, bench.what, redgem::bench::summarize(std::move(samples))};
}

bool wanted(const BenchCase& bench, const Options& options) {
  return options.only.empty() || bench.name.find(options.only) != std::string::npos;
}

std::vector<Result> run_all(const Options& options) {
  std::vector<Result> results;
  for (const BenchCase& bench : redgem::bench::all_cases()) {
    if (!wanted(bench, options)) continue;
    std::fprintf(stderr, "running %s\n", bench.name.c_str());
    results.push_back(run_case(bench, options));
  }
  return results;
}

int report(const std::vector<Result>& results, const Options& options) {
  std::fputs(redgem::bench::results_table(results).c_str(), stdout);
  std::string error;
  if (options.out.empty() || redgem::bench::write_results(options.out, kSuite, results, error)) return 0;
  std::fprintf(stderr, "%s\n", error.c_str());
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = read_options(argc, argv);
    return report(run_all(options), options);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "redgem_bench: %s\n", error.what());
    return 1;
  }
}
