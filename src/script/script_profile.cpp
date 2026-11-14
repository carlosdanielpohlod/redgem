#include "script/script_profile.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace redgem {
namespace {

constexpr int kSharePrecision = 1;  // tenths of a percent
constexpr const char* kReportHeader = "self_percent\ttotal_percent\tself_samples\ttotal_samples\tmethod\n";
constexpr const char* kReportSamples = "# samples: ";
constexpr const char* kReportDropped = "# dropped: ";

bool ranks_before(const ScriptMethodStat& left, const ScriptMethodStat& right) {
  if (left.self != right.self) return left.self > right.self;
  if (left.total != right.total) return left.total > right.total;
  return left.name < right.name;
}

bool counted(const std::vector<std::string>& seen, const std::string& name) {
  return std::find(seen.begin(), seen.end(), name) != seen.end();
}

std::string percent(double value) {
  std::ostringstream text;
  text << std::fixed << std::setprecision(kSharePrecision) << value;
  return text.str();
}

void report_row(std::ostringstream& out, const ScriptMethodStat& method, std::size_t samples) {
  out << percent(share_percent(method.self, samples)) << '\t' << percent(share_percent(method.total, samples)) << '\t'
      << method.self << '\t' << method.total << '\t' << method.name << '\n';
}

}  // namespace

ScriptMethodStat* ScriptProfile::stat(const std::string& name) {
  const auto found = methods_.find(name);
  if (found != methods_.end()) return &found->second;
  if (methods_.size() >= kMaxProfiledMethods) return nullptr;
  ScriptMethodStat& added = methods_[name];
  added.name = name;
  return &added;
}

// Whether the method was kept: a method past the limit is not.
bool ScriptProfile::count_total(const std::string& name) {
  ScriptMethodStat* method = stat(name);
  if (method == nullptr) return false;
  if (counted(seen_, name)) return true;
  ++method->total;
  seen_.push_back(name);
  return true;
}

void ScriptProfile::record(const std::vector<std::string>& stack) {
  if (stack.empty()) return;
  ++samples_;
  seen_.clear();
  bool dropped = false;
  for (const std::string& name : stack) dropped = !count_total(name) || dropped;
  if (ScriptMethodStat* innermost = stat(stack.back())) ++innermost->self;
  if (dropped) ++dropped_;
}

void ScriptProfile::clear() {
  methods_.clear();
  samples_ = 0;
  dropped_ = 0;
}

std::vector<ScriptMethodStat> ScriptProfile::top(std::size_t count) const {
  std::vector<ScriptMethodStat> ranked;
  ranked.reserve(methods_.size());
  for (const auto& entry : methods_) ranked.push_back(entry.second);
  const std::size_t kept = std::min(count, ranked.size());
  std::partial_sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(kept), ranked.end(), ranks_before);
  ranked.resize(kept);
  return ranked;
}

double share_percent(std::size_t part, std::size_t whole) {
  if (whole == 0) return 0.0;
  return kFullShare * static_cast<double>(part) / static_cast<double>(whole);
}

std::string script_method_line(const ScriptMethodStat& method, std::size_t samples) {
  return method.name + " " + percent(share_percent(method.self, samples)) + "% self, " +
         percent(share_percent(method.total, samples)) + "% total";
}

std::string script_profile_report(const ScriptProfile& profile) {
  std::ostringstream out;
  out << kReportSamples << profile.samples() << '\n' << kReportDropped << profile.dropped() << '\n' << kReportHeader;
  for (const ScriptMethodStat& method : profile.top(kMaxProfiledMethods)) report_row(out, method, profile.samples());
  return out.str();
}

}  // namespace redgem
