#include "bench/bench_results.hpp"

#include <cstdio>
#include <fstream>
#include <system_error>

namespace redgem::bench {

namespace {

constexpr int kJsonIndent = 2;
constexpr std::size_t kLineBytes = 256;

nlohmann::json result_json(const Result& result) {
  const Summary& summary = result.summary;
  return {{"unit", result.unit}, {"what", result.what}, {"samples", summary.samples}, {"median", summary.median},
          {"p95", summary.p95},  {"min", summary.min},  {"max", summary.max}};
}

std::string table_line(const Result& result) {
  const Summary& summary = result.summary;
  char line[kLineBytes];
  std::snprintf(line, sizeof(line), "| %s | %.3f | %.3f | %s | %zu | %s |", result.name.c_str(), summary.median,
                summary.p95, result.unit.c_str(), summary.samples, result.what.c_str());
  return line;
}

}  // namespace

nlohmann::json results_json(const std::string& suite, const std::vector<Result>& results) {
  nlohmann::json measured = nlohmann::json::object();
  for (const Result& result : results) measured[result.name] = result_json(result);
  return {{"suite", suite}, {"results", std::move(measured)}};
}

std::string results_table(const std::vector<Result>& results) {
  std::string table = "| name | median | p95 | unit | samples | one sample |\n|---|---|---|---|---|---|\n";
  for (const Result& result : results) table += table_line(result) + "\n";
  return table;
}

bool write_results(const std::filesystem::path& file, const std::string& suite, const std::vector<Result>& results,
                   std::string& error) {
  std::error_code ignored;
  if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ignored);
  std::ofstream out(file);
  out << results_json(suite, results).dump(kJsonIndent) << "\n";
  if (out.good()) return true;
  error = "cannot write " + file.string();
  return false;
}

}  // namespace redgem::bench
