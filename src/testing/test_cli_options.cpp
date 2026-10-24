// redgem-test's command line: its options, and what each one says.

#include <algorithm>
#include <iterator>
#include <optional>

#include "core/command_line.hpp"
#include "core/files.hpp"
#include "core/translations.hpp"
#include "render/gpu_select.hpp"
#include "testing/test_cli_reading.hpp"

namespace redgem::testing::cli {
namespace {

constexpr const char* kRubyOption = "--ruby";
constexpr const char* kScreenshotsOption = "--screenshots";
constexpr const char* kHeadlessOption = "--headless";
constexpr const char* kGpuOption = "--gpu";
constexpr const char* kWorkOption = "--work";
constexpr const char* kGameOption = "--game";
constexpr const char* kAllOption = "--all";
constexpr const char* kAfterOption = "--after";

// What the option just read was given (see command_line::option_value).
std::optional<std::string> value(Reading& reading) { return command_line::option_value(reading.args, reading.next); }

void read_help(Reading& reading) {
  reading.result.options = TestCliOptions{};
  reading.result.options.mode = TestMode::Help;  // help wins over anything else on the line
  reading.result.ok = true;
  reading.done = true;
}

void read_ruby(Reading& reading) {
  const auto file = value(reading);
  if (!file) {
    fail(reading, tr("testing.cli.ruby_needs_file"));
    return;
  }
  reading.result.options.mode = TestMode::RubyTests;
  reading.result.options.test_file = *file;
}

void read_screenshots(Reading& reading) {
  const auto folder = value(reading);
  if (!folder) {
    fail(reading, tr("testing.cli.screenshots_need_directory"));
    return;
  }
  reading.result.options.screenshot_dir = *folder;
}

void read_work(Reading& reading) {
  const auto folder = value(reading);
  if (!folder) {
    fail(reading, tr("testing.cli.work_needs_directory"));
    return;
  }
  reading.result.options.work_dir = *folder;
}

void read_after(Reading& reading) {
  const auto file = value(reading);
  if (!file) {
    fail(reading, tr("testing.cli.after_needs_test"));
    return;
  }
  reading.result.options.after = *file;
}

void read_game(Reading& reading) {
  const auto folder = value(reading);
  if (!folder) {
    fail(reading, tr("testing.cli.game_needs_folder"));
    return;
  }
  reading.result.options.scripts_game = normalized_folder(*folder);
}

// --all <game_dir>: every test of the game.
void read_all(Reading& reading) {
  const auto folder = value(reading);
  if (!folder) {
    fail(reading, tr("testing.cli.all_needs_game"));
    return;
  }
  reading.result.options.mode = TestMode::GameTests;
  reading.result.options.game_dir = normalized_folder(*folder);
}

void read_headless(Reading& reading) { reading.result.options.headless = true; }

void read_gpu(Reading& reading) {
  // (No name is no card's name either.)
  const std::string name = value(reading).value_or("");
  if (!gpu_choice_from_name(name)) {
    fail(reading, tr("testing.cli.gpu_needs_card"));
    return;
  }
  reading.result.options.gpu = name;
}

constexpr Option kOptions[] = {
    {command_line::kHelpOption, read_help},
    {command_line::kShortHelpOption, read_help},
    {kRubyOption, read_ruby},
    {kScreenshotsOption, read_screenshots},
    {kHeadlessOption, read_headless},
    {kGpuOption, read_gpu},
    {kWorkOption, read_work},
    {kGameOption, read_game},
    {kAllOption, read_all},
    {kAfterOption, read_after},
};

}  // namespace

const Option* find_option(const std::string& word) {
  const auto* found = std::find_if(std::begin(kOptions), std::end(kOptions),
                                   [&word](const Option& option) { return word == option.name; });
  return found != std::end(kOptions) ? found : nullptr;
}

}  // namespace redgem::testing::cli
