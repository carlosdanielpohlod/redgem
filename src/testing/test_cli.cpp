#include "testing/test_cli.hpp"

#include <algorithm>
#include <iterator>

#include "core/files.hpp"
#include "app/user_settings.hpp"
#include "core/config.hpp"
#include "testing/testing_config.hpp"

namespace redgem::testing {
namespace {

constexpr char kOptionMark = '-';  // what every option starts with

// The words of a command line.
constexpr const char* kHelpOption = "--help";
constexpr const char* kShortHelpOption = "-h";
constexpr const char* kRubyOption = "--ruby";
constexpr const char* kScreenshotsOption = "--screenshots";
constexpr const char* kHeadlessOption = "--headless";
constexpr const char* kGpuOption = "--gpu";

constexpr const char* kUsageText =
    "redgem-test runs the tests of a game. It is a development tool: built games do not have it.\n"
    "\n"
    "usage:\n"
    "  redgem-test <game_dir> <system_test.rb> [--screenshots <dir>] [--headless]\n"
    "                                    play the game with a system test at the controls\n"
    "  redgem-test --ruby <test_file.rb>    run a file of Ruby unit tests\n"
    "\n"
    "  --screenshots <dir>   where the test's pictures go (default: screenshots)\n"
    "  --headless            no window: faster, and takes no pictures\n"
    "  --gpu auto|dedicated|integrated      graphics card for this run (default: the saved setting)\n"
    "\n"
    "It exits with 0 when the tests pass and with 1 when they do not.\n";

// A command line being read: what was understood so far, and how far the reading got.
struct Reading {
  // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
  const std::vector<std::string>& args;
  std::size_t next = 0;  // the argument to read next
  TestCliResult result;
  bool done = false;  // an error or --help ended the reading
};

void fail(Reading& reading, const std::string& message) {
  reading.result.ok = false;
  reading.result.error = message;
  reading.done = true;
}

bool is_option(const std::string& arg) { return !arg.empty() && arg[0] == kOptionMark; }

// What the option just read was given: the next argument, unless it is missing or is itself an option.
std::optional<std::string> value(Reading& reading) {
  if (reading.next >= reading.args.size() || is_option(reading.args[reading.next])) return std::nullopt;
  return reading.args[reading.next++];
}

void read_help(Reading& reading) {
  reading.result.options = TestCliOptions{};
  reading.result.options.mode = TestMode::Help;  // help wins over anything else on the line
  reading.result.ok = true;
  reading.done = true;
}

void read_ruby(Reading& reading) {
  const auto file = value(reading);
  if (!file) {
    fail(reading, "--ruby needs a file");
    return;
  }
  reading.result.options.mode = TestMode::RubyTests;
  reading.result.options.test_file = *file;
}

void read_screenshots(Reading& reading) {
  const auto folder = value(reading);
  if (!folder) {
    fail(reading, "--screenshots needs a directory");
    return;
  }
  reading.result.options.screenshot_dir = *folder;
}

void read_headless(Reading& reading) { reading.result.options.headless = true; }

void read_gpu(Reading& reading) {
  const auto name = value(reading);
  if (!name || !gpu_choice_from_name(*name)) {
    fail(reading, "--gpu needs auto, dedicated or integrated");
    return;
  }
  reading.result.options.gpu = *name;
}

struct Option {
  const char* name;
  void (*read)(Reading&);
};
constexpr Option kOptions[] = {
    {kHelpOption, read_help},         {kShortHelpOption, read_help},
    {kRubyOption, read_ruby},         {kScreenshotsOption, read_screenshots},
    {kHeadlessOption, read_headless}, {kGpuOption, read_gpu},
};

const Option* find_option(const std::string& arg) {
  const auto* found = std::find_if(std::begin(kOptions), std::end(kOptions),
                                   [&arg](const Option& option) { return arg == option.name; });
  return found != std::end(kOptions) ? found : nullptr;
}

// An argument that is no option: the game's folder, then the system test's file.
void read_path(Reading& reading, const std::string& arg) {
  TestCliOptions& options = reading.result.options;
  if (options.game_dir.empty()) {
    options.game_dir = normalized_folder(arg);
  } else if (options.test_file.empty()) {
    options.test_file = arg;
  } else {
    fail(reading, "unexpected argument " + arg);
  }
}

void read_argument(Reading& reading) {
  const std::string& arg = reading.args[reading.next++];
  if (const Option* option = find_option(arg)) {
    option->read(reading);
  } else if (is_option(arg)) {
    fail(reading, "unknown option " + arg);
  } else {
    read_path(reading, arg);
  }
}

// What is wrong with a line that was read to its end; empty when nothing is.
std::string problem_with(const TestCliOptions& options) {
  const bool ruby = options.mode == TestMode::RubyTests;
  if (ruby && !options.game_dir.empty()) return "--ruby runs one file of Ruby tests, without a game";
  if (ruby && options.headless) return "--headless is for system tests (Ruby tests never open a window)";
  if (ruby) return {};
  if (options.game_dir.empty()) return "missing <game_dir>";
  if (options.test_file.empty()) return "missing <system_test.rb>";
  return {};
}

}  // namespace

std::string test_usage() { return kUsageText; }

TestCliResult parse_test_cli(const std::vector<std::string>& args) {
  Reading reading{args};
  reading.result.options.screenshot_dir = config::kDefaultScreenshotDir;
  while (!reading.done && reading.next < args.size()) read_argument(reading);
  if (reading.done) return reading.result;
  reading.result.error = problem_with(reading.result.options);
  reading.result.ok = reading.result.error.empty();
  return reading.result;
}

}  // namespace redgem::testing
