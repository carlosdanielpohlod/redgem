#include "testing/test_cli.hpp"

#include "core/command_line.hpp"
#include "core/files.hpp"
#include "testing/test_cli_reading.hpp"
#include "testing/testing_config.hpp"

namespace redgem::testing {

namespace cli {

void fail(Reading& reading, const std::string& message) {
  reading.result.ok = false;
  reading.result.error = message;
  reading.done = true;
}

}  // namespace cli

namespace {

using cli::Reading;

constexpr const char* kUsageText =
    "redgem-test runs the tests of a game. It is a development tool: built games do not have it.\n"
    "\n"
    "usage:\n"
    "  redgem-test <game_dir> <system_test.rb> [--screenshots <dir>] [--headless] [--work <dir>]\n"
    "                                    play the game with a system test at the controls\n"
    "  redgem-test --ruby <test_file.rb>    run a file of Ruby unit tests\n"
    "\n"
    "  --screenshots <dir>   where the test's pictures go (default: screenshots)\n"
    "  --headless            no window: faster, and takes no pictures\n"
    "  --work <dir>          a folder of this run's own for its save file and its notes to the tests\n"
    "                        run with it (default: folders every run on this machine shares)\n"
    "  --gpu auto|dedicated|integrated      graphics card for this run (default: the saved setting)\n"
    "\n"
    "It exits with 0 when the tests pass and with 1 when they do not.\n";

// An argument that is no option: the game's folder, then the system test's file.
void read_path(Reading& reading, const std::string& arg) {
  TestCliOptions& options = reading.result.options;
  if (options.game_dir.empty()) {
    options.game_dir = normalized_folder(arg);
  } else if (options.test_file.empty()) {
    options.test_file = arg;
  } else {
    cli::fail(reading, "unexpected argument " + arg);
  }
}

void read_argument(Reading& reading) {
  const std::string& arg = reading.args[reading.next++];
  if (const cli::Option* option = cli::find_option(arg)) {
    option->read(reading);
  } else if (command_line::is_option(arg)) {
    cli::fail(reading, "unknown option " + arg);
  } else {
    read_path(reading, arg);
  }
}

// What is wrong with a line that asks for a file of Ruby tests; empty when nothing is.
std::string problem_with_ruby_tests(const TestCliOptions& options) {
  if (!options.game_dir.empty()) return "--ruby runs one file of Ruby tests, without a game";
  if (options.headless) return "--headless is for system tests (Ruby tests never open a window)";
  if (!options.work_dir.empty()) return "--work is for system tests (Ruby tests leave nothing behind)";
  return {};
}

// What is wrong with a line that was read to its end; empty when nothing is.
std::string problem_with(const TestCliOptions& options) {
  if (options.mode == TestMode::RubyTests) return problem_with_ruby_tests(options);
  if (options.game_dir.empty()) return "missing <game_dir>";
  if (options.test_file.empty()) return "missing <system_test.rb>";
  return {};
}

}  // namespace

std::string test_usage() { return kUsageText; }

TestCliResult parse_test_cli(const std::vector<std::string>& args) {
  Reading reading(args);
  reading.result.options.screenshot_dir = config::kDefaultScreenshotDir;
  while (reading.more()) read_argument(reading);
  if (reading.done) return reading.result;
  reading.result.error = problem_with(reading.result.options);
  reading.result.ok = reading.result.error.empty();
  return reading.result;
}

}  // namespace redgem::testing
