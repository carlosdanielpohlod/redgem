// redgem-test's command line: its options, and what each one says.

#include <algorithm>
#include <iterator>
#include <optional>

#include "core/command_line.hpp"
#include "core/files.hpp"
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

void read_work(Reading& reading) {
  const auto folder = value(reading);
  if (!folder) {
    fail(reading, "--work needs a directory");
    return;
  }
  reading.result.options.work_dir = *folder;
}

void read_game(Reading& reading) {
  const auto folder = value(reading);
  if (!folder) {
    fail(reading, "--game needs a game's folder");
    return;
  }
  reading.result.options.scripts_game = normalized_folder(*folder);
}

void read_headless(Reading& reading) { reading.result.options.headless = true; }

void read_gpu(Reading& reading) {
  // (No name is no card's name either.)
  const std::string name = value(reading).value_or("");
  if (!gpu_choice_from_name(name)) {
    fail(reading, "--gpu needs auto, dedicated or integrated");
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
};

}  // namespace

const Option* find_option(const std::string& word) {
  const auto* found = std::find_if(std::begin(kOptions), std::end(kOptions),
                                   [&word](const Option& option) { return word == option.name; });
  return found != std::end(kOptions) ? found : nullptr;
}

}  // namespace redgem::testing::cli
