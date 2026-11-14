#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "bench/bench_stats.hpp"

// What the benchmarks (redgem_bench) and the server load test (redgem_server_load) measured, in
// the form tools/bench/check_thresholds.py reads and the CI keeps:
//
//   {"suite": "bench", "results": {"<name>": {"unit": "us", "what": "...", "samples": 30,
//                                              "median": 1.5, "p95": 2.0, "min": 1.4, "max": 2.2}}}
namespace redgem::bench {

struct Result {
  std::string name;  // what thresholds.json calls it
  std::string unit;
  std::string what;  // what one sample is, for the table
  Summary summary;
};

nlohmann::json results_json(const std::string& suite, const std::vector<Result>& results);
// The results as a short Markdown table, one line each.
std::string results_table(const std::vector<Result>& results);
// Writes results_json to `file`, making its folder; false with `error` when it cannot.
bool write_results(const std::filesystem::path& file, const std::string& suite, const std::vector<Result>& results,
                   std::string& error);

// Prints results_table, and writes the results to `file` unless it is empty: the exit code of a
// program that measured them (0, or 1 when they could not be written).
int report_results(const std::string& suite, const std::vector<Result>& results, const std::filesystem::path& file);

}  // namespace redgem::bench
