#include "project/check_command.hpp"

#include <filesystem>

#include "core/command_line.hpp"
#include "core/translations.hpp"
#include "project/game_checks.hpp"
#include "project/game_tests.hpp"
#include "project/scene_command.hpp"
#include "project/tool_play.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kTestsOption = "--run-tests";
constexpr const char* kAllOption = "--all";
// What the command takes after its name.
ToolGrammar grammar() { return {{}, {kTestsOption, kAllOption, command_line::kHelpOption}}; }
// How many problems a check lists unless --all: a screenful.
constexpr std::size_t kMostLines = 30;
constexpr const char* kErrorWord = "error ";
constexpr const char* kWarningWord = "warning ";
constexpr const char* kFailWord = "FAIL ";

// The lines of a check, as many as it may show.
class CheckLines {
 public:
  explicit CheckLines(bool all) : all_(all) {}

  void add(const char* kind, const std::vector<std::string>& lines) {
    for (const std::string& line : lines) add(kind + line);
  }
  void add(const std::string& line) {
    if (all_ || shown_ < kMostLines) {
      text_ += line + "\n";
      ++shown_;
    } else {
      ++left_out_;
    }
  }
  std::string text() const {
    if (left_out_ == 0) return text_;
    return text_ + tr("project.check.more", {{"count", std::to_string(left_out_)}}) + "\n";
  }

 private:
  bool all_;
  std::string text_;
  std::size_t shown_ = 0;
  std::size_t left_out_ = 0;
};

std::string tests_line(const TestsRun& tests) {
  return tr("project.check.tests",
            {{"passed", std::to_string(tests.passed)}, {"failed", std::to_string(tests.failures.size())}});
}

std::string summary(const CheckReport& report) {
  return tr("project.check.summary", {{"errors", std::to_string(report.errors.size())},
                                      {"warnings", std::to_string(report.warnings.size())},
                                      {"scripts", std::to_string(report.scripts)},
                                      {"scenes", std::to_string(report.scenes)}});
}

// The game's tests, run once its files are in order (a test of a broken game says nothing new).
std::optional<TestsRun> tests_if_asked(const ToolLine& line, const CheckReport& report) {
  if (!line.has(kTestsOption) || !report.errors.empty()) return std::nullopt;
  ScratchFolder scratch;
  return run_game_tests(line.words.front(), tester_beside_this_program(), scratch.path());
}

std::string problem_lines(const CheckReport& report, const std::optional<TestsRun>& tests, bool all) {
  CheckLines lines(all);
  lines.add(kErrorWord, report.errors);
  lines.add(kWarningWord, report.warnings);
  if (tests) lines.add(kFailWord, tests->failures);
  if (tests && !tests->problem.empty()) lines.add(tests->problem);
  return lines.text();
}

bool went_wrong(const CheckReport& report, const std::optional<TestsRun>& tests) {
  return !report.errors.empty() || (tests && (!tests->failures.empty() || !tests->problem.empty()));
}

ToolResult checked(const ToolLine& line) {
  CheckReport report;
  check_scripts(line.words.front(), report);
  check_scenes(line.words.front(), report);
  const std::optional<TestsRun> tests = tests_if_asked(line, report);
  ToolResult result{kExitSuccess, problem_lines(report, tests, line.has(kAllOption)), {}};
  if (tests) result.out += tests_line(*tests) + "\n";
  result.out += summary(report) + "\n";
  if (went_wrong(report, tests)) result.exit_code = kExitFailure;
  return result;
}

ToolResult usage_failure(const std::string& problem) {
  return failed(problem + "\n\n" + tr("project.check.usage"), kExitUsage);
}

}  // namespace

ToolResult run_check_command(const std::vector<std::string>& args) {
  std::string error;
  const auto line = read_tool_line(args, 0, grammar(), error);
  if (!line) return usage_failure(error);
  if (line->has(command_line::kHelpOption)) return ToolResult{kExitSuccess, tr("project.check.usage"), {}};
  if (line->words.size() != 1) return usage_failure(tr("project.tools.needs_game"));
  if (scene_names(line->words.front()).empty()) {
    return failed(tr("project.tools.not_a_game", {{"game_dir", line->words.front()}}));
  }
  return checked(*line);
}

}  // namespace redgem::tools
