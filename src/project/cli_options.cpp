// redgem's command line: its options, and what each one says.

#include <algorithm>
#include <iterator>
#include <optional>

#include "core/command_line.hpp"
#include "project/cli_reading.hpp"
#include "project/game_build.hpp"
#include "render/gpu_select.hpp"

namespace redgem::cli {
namespace {

constexpr const char* kPlayOption = "--play";
constexpr const char* kTemplateOption = "--template";
constexpr const char* kTargetOption = "--target";
constexpr const char* kGpuOption = "--gpu";

// What the option just read was given (see command_line::option_value).
std::optional<std::string> value(Reading& reading) { return command_line::option_value(reading.args, reading.next); }

void read_help(Reading& reading) {
  reading.result.options = CliOptions{};
  reading.result.options.mode = RunMode::Help;  // help wins over anything else on the line
  reading.result.ok = true;
  reading.done = true;
}

void read_play(Reading& reading) { choose(reading, RunMode::Game); }

void read_template(Reading& reading) {
  const auto name = value(reading);
  if (!name) {
    fail(reading, "--template needs a name");
    return;
  }
  reading.result.options.template_name = *name;
}

void read_target(Reading& reading) {
  // (No name is no target's name either.)
  const std::string name = value(reading).value_or("");
  if (!build_target_from_name(name)) {
    fail(reading, "--target needs linux, windows, macos or android");
    return;
  }
  reading.result.options.build_target = name;
}

void read_gpu(Reading& reading) {
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
    {kPlayOption, read_play},
    {kTemplateOption, read_template},
    {kTargetOption, read_target},
    {kGpuOption, read_gpu},
};

}  // namespace

const Option* find_option(const std::string& word) {
  const auto* found = std::find_if(std::begin(kOptions), std::end(kOptions),
                                   [&word](const Option& option) { return word == option.name; });
  return found != std::end(kOptions) ? found : nullptr;
}

}  // namespace redgem::cli
