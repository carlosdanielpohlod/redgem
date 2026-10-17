#include "testing/test_cli.hpp"

#include "core/command_line.hpp"
#include "core/files.hpp"
#include "core/translations.hpp"
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

// An argument that is no option: the game's folder, then the system test's file.
void read_path(Reading& reading, const std::string& arg) {
  TestCliOptions& options = reading.result.options;
  if (options.game_dir.empty()) {
    options.game_dir = normalized_folder(arg);
  } else if (options.test_file.empty()) {
    options.test_file = arg;
  } else {
    cli::fail(reading, tr("testing.cli.unexpected_argument", {{"argument", arg}}));
  }
}

void read_argument(Reading& reading) {
  const std::string& arg = reading.args[reading.next++];
  if (const cli::Option* option = cli::find_option(arg)) {
    option->read(reading);
  } else if (command_line::is_option(arg)) {
    cli::fail(reading, tr("testing.cli.unknown_option", {{"option", arg}}));
  } else {
    read_path(reading, arg);
  }
}

// What is wrong with a line that asks for a file of Ruby tests; empty when nothing is.
std::string problem_with_ruby_tests(const TestCliOptions& options) {
  if (!options.game_dir.empty()) return tr("testing.cli.ruby_without_game");
  if (options.headless) return tr("testing.cli.headless_for_system_tests");
  if (!options.work_dir.empty()) return tr("testing.cli.work_for_system_tests");
  return {};
}

// What is wrong with a line that was read to its end; empty when nothing is.
std::string problem_with(const TestCliOptions& options) {
  if (options.mode == TestMode::RubyTests) return problem_with_ruby_tests(options);
  if (!options.scripts_game.empty()) return tr("testing.cli.game_for_ruby_tests");
  if (options.game_dir.empty()) return tr("testing.cli.missing_game_dir");
  if (options.test_file.empty()) return tr("testing.cli.missing_system_test");
  return {};
}

}  // namespace

std::string test_usage() { return tr("testing.cli.usage", {{"screenshot_dir", config::kDefaultScreenshotDir}}); }

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
