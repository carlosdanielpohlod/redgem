#include "project/game_tests.hpp"

#include <cstdlib>
#include <sstream>

#include "app/recording.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

constexpr const char* kTesterName = "redgem-test";
// What redgem-test is asked, and how it says how each file went (see testing/game_test_suite.hpp).
constexpr const char* kAllOption = "--all";
constexpr const char* kWorkOption = "--work";
constexpr const char* kPassedMark = "PASS ";
constexpr const char* kFailedMark = "FAIL ";
constexpr const char* kOutputFile = "tests-said.txt";
constexpr const char* kWorkFolder = "work";
constexpr int kSucceeded = 0;  // what the shell says of a program that ended well

bool starts_with(const std::string& text, const char* start) { return text.rfind(start, 0) == 0; }

// The line a shell runs: the program, its arguments, and what it prints into `output` (what it
// logs, the details, goes nowhere).
std::string command_line(const std::vector<std::string>& words, const fs::path& output) {
  const bool windows = platform::this_desktop() == platform::Desktop::Windows;
  const std::string nowhere = windows ? "NUL" : "/dev/null";
  const std::string command = shell_command(words) + " > " + shell_command({output.string()}) + " 2> " + nowhere;
  // (Windows' shell drops the first and last quotes of a line that starts with one.)
  return windows ? "\"" + command + "\"" : command;
}

// Whether redgem-test failed without saying which file did (it crashed, or did not understand
// what it was asked): no test can then be taken as passed.
bool ended_unsaid(int status, const TestsRun& run) { return status != kSucceeded && run.failures.empty(); }

}  // namespace

TestsRun read_tests_run(const std::string& said) {
  TestsRun run;
  std::stringstream stream(said);
  for (std::string line; std::getline(stream, line);) {
    if (starts_with(line, kPassedMark)) ++run.passed;
    if (starts_with(line, kFailedMark)) run.failures.push_back(line.substr(std::string(kFailedMark).size()));
  }
  return run;
}

fs::path tester_beside_this_program() {
  const bool windows = platform::this_desktop() == platform::Desktop::Windows;
  return platform::executable_path().parent_path() / platform::executable_file_name(kTesterName, windows);
}

TestsRun run_game_tests(const fs::path& game_dir, const fs::path& tester, const fs::path& scratch) {
  if (!fs::exists(tester)) return TestsRun{0, {}, tr("project.check.no_tester", {{"tester", tester.string()}})};
  const fs::path output = scratch / kOutputFile;
  const std::vector<std::string> words{tester.string(), kAllOption, game_dir.string(), kWorkOption,
                                       (scratch / kWorkFolder).string()};
  const int status = std::system(command_line(words, output).c_str());  // NOLINT(bugprone-command-processor)
  std::string error;
  TestsRun run = read_tests_run(read_text_file(output, error).value_or(""));
  if (ended_unsaid(status, run)) run.problem = tr("project.check.tester_failed", {{"status", std::to_string(status)}});
  return run;
}

}  // namespace redgem::tools
