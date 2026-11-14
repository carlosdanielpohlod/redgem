#include "bench/bench_case.hpp"

#include <stdexcept>

namespace redgem::bench {

namespace {

volatile std::size_t g_kept = 0;

}  // namespace

void keep(std::size_t value) { g_kept = g_kept + value; }

void require(bool worked, const std::string& what) {
  if (!worked) throw std::runtime_error(what + " failed");
}

std::vector<BenchCase> all_cases() {
  std::vector<BenchCase> cases = {script_case(), physics_case(), scene_case()};
  for (BenchCase& one : network_cases()) cases.push_back(std::move(one));
  for (BenchCase& one : save_cases()) cases.push_back(std::move(one));
  cases.push_back(http_case());
  return cases;
}

}  // namespace redgem::bench
