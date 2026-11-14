#pragma once

// What the sampling profiler of the scripts found (see script_sampler.hpp): for each method seen
// on the scripts' call stack, how many samples caught it running its own code (self) and how many
// caught it anywhere on the stack (total, once a sample however often it recurses). No mruby here.

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace redgem {

// Methods beyond this many are not kept: a game that makes methods without end cannot grow it.
inline constexpr std::size_t kMaxProfiledMethods = 1024;
inline constexpr double kFullShare = 100.0;  // percent

struct ScriptMethodStat {
  std::string name;  // "Class#method", "Class.method" for a class's own, as script_sampler names it
  std::size_t self = 0;
  std::size_t total = 0;
};

class ScriptProfile {
 public:
  // One sample: the methods on the stack, outermost first. An empty stack counts for nothing.
  void record(const std::vector<std::string>& stack);
  void clear();
  std::size_t samples() const { return samples_; }
  // Samples that met a method past kMaxProfiledMethods (counted without that method).
  std::size_t dropped() const { return dropped_; }
  // The `count` methods with the most self samples (ties: most total, then by name).
  std::vector<ScriptMethodStat> top(std::size_t count) const;

 private:
  ScriptMethodStat* stat(const std::string& name);
  bool count_total(const std::string& name);

  std::unordered_map<std::string, ScriptMethodStat> methods_;
  std::vector<std::string> seen_;  // the methods counted in the sample being recorded
  std::size_t samples_ = 0;
  std::size_t dropped_ = 0;
};

// The percentage `part` is of `whole`; zero of nothing.
double share_percent(std::size_t part, std::size_t whole);
// One method as the overlay shows it: "Player#update 41.5% self, 52.0% total".
std::string script_method_line(const ScriptMethodStat& method, std::size_t samples);
// The profile as a file of tab-separated columns (a header line, then a method a line, most self
// first): what Debug.export_script_profile writes.
std::string script_profile_report(const ScriptProfile& profile);

}  // namespace redgem
