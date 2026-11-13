#include "project/run_plan.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/safe_path.hpp"
#include "core/translations.hpp"
#include "project/tool_play.hpp"

namespace redgem::tools {
namespace {

constexpr std::uint32_t kLargestSeed = 2147483647;  // also fits an interpreter with 32-bit integers
constexpr std::uintmax_t kMostInputBytes = 1024 * 1024;
constexpr std::size_t kMostEvents = 10000;
constexpr std::size_t kMostExpressionBytes = 4096;
constexpr int kMostInputDepth = 5;  // events, an event, its pads, a pad, its axes

std::optional<long long> whole_number(const std::string& text, long long least, long long most) {
  const auto number = number_from(text);
  if (!number || *number < static_cast<double>(least) || *number > static_cast<double>(most) ||
      std::floor(*number) != *number)
    return std::nullopt;
  return static_cast<long long>(*number);
}

std::string read_numbers(const ToolLine& line, RunPlan& plan) {
  const auto seed = whole_number(line.value(kSeedOption).value_or("0"), 0, kLargestSeed);
  if (!seed) return tr("project.run.bad_seed");
  const auto every = whole_number(line.value(kEveryOption).value_or(std::to_string(kDefaultPrintEvery)), 1, kMostSteps);
  if (!every) return tr("project.run.bad_every");
  if (line.value(kEveryOption) && !line.value(kPrintOption)) return tr("project.run.every_needs_print");
  plan.seed = static_cast<std::uint32_t>(*seed);
  plan.every = *every;
  return {};
}

std::string read_events(const nlohmann::json& json, long long frames, std::vector<RunInput>& events) {
  if (!json.is_array() || json.size() > kMostEvents) return tr("project.run.bad_events");
  for (const auto& value : json) {
    RunInput event;
    const std::string problem = read_run_input(value, frames, event);
    if (!problem.empty())
      return tr("project.run.input_event", {{"event", std::to_string(events.size())}, {"problem", problem}});
    events.push_back(std::move(event));
  }
  return {};
}

// A click or a tap is let go of on the frame after it: an event on that frame would hide the release
// (two clicks in a row would read as one long press).
bool crowds_pulse(const RunInput& before, const RunInput& event) {
  return event.frame == before.frame + 1 && lasts_one_frame(before);
}

std::string spacing_problem(const RunInput& before, const RunInput& event) {
  const std::string frame = std::to_string(event.frame);
  if (event.frame == before.frame) return tr("project.run.duplicate_frame", {{"frame", frame}});
  if (crowds_pulse(before, event)) return tr("project.run.crowded_pulse", {{"frame", frame}});
  if (hides_press(before, event)) return tr("project.run.hidden_press", {{"frame", frame}});
  return {};
}

std::string order_events(std::vector<RunInput>& events) {
  std::sort(events.begin(), events.end(), [](const RunInput& a, const RunInput& b) { return a.frame < b.frame; });
  for (std::size_t i = 1; i < events.size(); ++i) {
    const std::string problem = spacing_problem(events[i - 1], events[i]);
    if (!problem.empty()) return problem;
  }
  return {};
}

nlohmann::json input_json(const std::vector<std::uint8_t>& bytes) {
  try {
    return nlohmann::json::parse(
        bytes.begin(), bytes.end(),
        [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
          if (depth > kMostInputDepth) throw std::length_error("input nesting");
          return true;
        },
        false);
  } catch (const std::length_error&) {
    return nlohmann::json(nlohmann::json::value_t::discarded);
  }
}

std::string read_input(const ToolLine& line, long long frames, RunPlan& plan) {
  const auto file = line.value(kInputOption);
  if (!file) return {};
  const auto bytes = read_file_up_to(*file, kMostInputBytes);
  if (!bytes) return tr("project.run.bad_input_file", {{"file", printable(*file)}});
  const std::string problem = read_events(input_json(*bytes), frames, plan.input);
  return problem.empty() ? order_events(plan.input) : problem;
}

std::string read_expressions(const ToolLine& line, RunPlan& plan) {
  plan.assertion = line.value(kAssertOption);
  plan.until = line.value(kUntilOption);
  plan.print = line.value(kPrintOption);
  for (const char* name : {kAssertOption, kUntilOption, kPrintOption}) {
    const auto expression = line.value(name);
    if (expression && (expression->empty() || expression->size() > kMostExpressionBytes))
      return tr("project.run.bad_expression", {{"option", name}});
  }
  return {};
}

}  // namespace

std::variant<RunPlan, std::string> read_run_plan(const ToolLine& line, long long frames) {
  RunPlan plan;
  for (const auto& problem : {read_numbers(line, plan), read_expressions(line, plan), read_input(line, frames, plan)}) {
    if (!problem.empty()) return problem;
  }
  return plan;
}

}  // namespace redgem::tools
