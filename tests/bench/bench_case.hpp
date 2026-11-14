#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// One benchmark of redgem_bench: a hot path of the engine that needs no window and no graphics
// card, timed over and over. Its state is made once (prepare); each sample then times a batch of
// the operation and gives the microseconds one of them took on average.
namespace redgem::bench {

// Takes one sample: the microseconds one operation took. Throws std::runtime_error when the
// operation did not do what it should (a benchmark of broken code measures nothing).
using Sampler = std::function<double()>;

struct BenchCase {
  std::string name;  // what thresholds.json calls it
  std::string what;  // what one operation is
  std::function<Sampler()> prepare;
};

// The microseconds each of `operations` calls of `operation` took, on average.
template <typename Operation>
double microseconds_each(int operations, Operation&& operation) {
  using Clock = std::chrono::steady_clock;
  const Clock::time_point start = Clock::now();
  for (int i = 0; i < operations; ++i) operation();
  const std::chrono::duration<double, std::micro> took = Clock::now() - start;
  return took.count() / operations;
}

// Keeps a result the compiler would otherwise see is never used, and leave out the work making it.
void keep(std::size_t value);
// Throws std::runtime_error saying `what` failed, when `worked` is false.
void require(bool worked, const std::string& what);

// The benchmarks, each from its own file.
BenchCase script_case();
BenchCase physics_case();
BenchCase scene_case();
std::vector<BenchCase> network_cases();
std::vector<BenchCase> save_cases();
BenchCase http_case();

// Every benchmark, in the order they run.
std::vector<BenchCase> all_cases();

}  // namespace redgem::bench
