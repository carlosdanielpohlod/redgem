// redgem's command line: its options, and what each one says.

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>

#include "app/recording.hpp"
#include "core/command_line.hpp"
#include "core/translations.hpp"
#include "project/cli_reading.hpp"
#include "project/game_build.hpp"
#include "render/gpu_select.hpp"

namespace redgem::cli {
namespace {

constexpr const char* kVersionOption = "--version";
constexpr const char* kPlayOption = "--play";
constexpr const char* kTemplateOption = "--template";
constexpr const char* kTargetOption = "--target";
constexpr const char* kGpuOption = "--gpu";
constexpr const char* kRecordOption = "--record";
constexpr const char* kRecordFpsOption = "--record-fps";
constexpr const char* kRecordSizeOption = "--record-size";
constexpr const char* kRecordScaleOption = "--record-scale";
constexpr const char* kRecordSecondsOption = "--record-seconds";

// What the option just read was given (see command_line::option_value).
std::optional<std::string> value(Reading& reading) { return command_line::option_value(reading.args, reading.next); }

// Help and the version win over anything else on the line: the rest of it is not read.
void answer_at_once(Reading& reading, RunMode mode) {
  reading.result.options = CliOptions{};
  reading.result.options.mode = mode;
  reading.result.ok = true;
  reading.done = true;
}

void read_help(Reading& reading) { answer_at_once(reading, RunMode::Help); }

void read_version(Reading& reading) { answer_at_once(reading, RunMode::Version); }

void read_play(Reading& reading) { choose(reading, RunMode::Game); }

void read_template(Reading& reading) {
  const auto name = value(reading);
  if (!name) {
    fail(reading, tr("project.cli.template_needs_name"));
    return;
  }
  reading.result.options.template_name = *name;
}

void read_target(Reading& reading) {
  // (No name is no target's name either.)
  const std::string name = value(reading).value_or("");
  if (!build_target_from_name(name)) {
    fail(reading, tr("project.cli.target_needs_system"));
    return;
  }
  reading.result.options.build_target = name;
}

void read_gpu(Reading& reading) {
  const std::string name = value(reading).value_or("");
  if (!gpu_choice_from_name(name)) {
    fail(reading, tr("project.cli.gpu_needs_card"));
    return;
  }
  reading.result.options.gpu = name;
}

// Recording plays the game, into a video.
void read_record(Reading& reading) {
  const auto file = value(reading);
  if (!file) {
    fail(reading, tr("project.cli.record_needs_file"));
    return;
  }
  reading.result.options.record.file = *file;
  choose(reading, RunMode::Game);
}

void read_record_fps(Reading& reading) {
  const auto fps = record_fps_from(value(reading).value_or(""));
  if (!fps) {
    fail(reading, tr("project.cli.record_fps_needs_number", {{"most", std::to_string(kMostRecordFps)}}));
    return;
  }
  reading.result.options.record.fps = *fps;
}

// --record-size and --record-scale: a size, into `place`.
void read_size(Reading& reading, std::optional<FrameSize>& place, const char* option) {
  const auto size = frame_size_from(value(reading).value_or(""));
  if (!size) {
    fail(reading, tr("project.cli.record_size_needs_size", {{"option", option}}));
    return;
  }
  place = size;
}

void read_record_size(Reading& reading) { read_size(reading, reading.result.options.record.size, kRecordSizeOption); }

void read_record_scale(Reading& reading) {
  read_size(reading, reading.result.options.record.scale, kRecordScaleOption);
}

void read_record_seconds(Reading& reading) {
  const auto seconds = record_seconds_from(value(reading).value_or(""));
  if (!seconds) {
    fail(reading, tr("project.cli.record_seconds_needs_number"));
    return;
  }
  reading.result.options.record.seconds = seconds;
}

constexpr Option kOptions[] = {
    {command_line::kHelpOption, read_help},
    {command_line::kShortHelpOption, read_help},
    {kVersionOption, read_version},
    {kPlayOption, read_play},
    {kTemplateOption, read_template},
    {kTargetOption, read_target},
    {kGpuOption, read_gpu},
    {kRecordOption, read_record},
    {kRecordFpsOption, read_record_fps},
    {kRecordSizeOption, read_record_size},
    {kRecordScaleOption, read_record_scale},
    {kRecordSecondsOption, read_record_seconds},
};

}  // namespace

const Option* find_option(const std::string& word) {
  const auto* found = std::find_if(std::begin(kOptions), std::end(kOptions),
                                   [&word](const Option& option) { return word == option.name; });
  return found != std::end(kOptions) ? found : nullptr;
}

}  // namespace redgem::cli
