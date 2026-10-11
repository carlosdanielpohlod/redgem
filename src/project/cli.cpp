#include "project/cli.hpp"

#include "core/files.hpp"
#include "project/game_build.hpp"
#include "app/user_settings.hpp"
#include "core/config.hpp"

#include <algorithm>
#include <iterator>

namespace redgem {

namespace {

constexpr std::size_t kPortDigits = 5;
constexpr int kLargestPort = 65535;
constexpr const char* kDigits = "0123456789";
constexpr char kOptionMark = '-';  // what every option starts with

// The words of a command line.
constexpr const char* kNewCommand = "new";
constexpr const char* kBuildCommand = "build";
constexpr const char* kRelayCommand = "relay";
constexpr const char* kHelpOption = "--help";
constexpr const char* kShortHelpOption = "-h";
constexpr const char* kPlayOption = "--play";
constexpr const char* kScreenshotsOption = "--screenshots";
constexpr const char* kTemplateOption = "--template";
constexpr const char* kTargetOption = "--target";
constexpr const char* kGpuOption = "--gpu";
constexpr const char* kPortOption = "--port";

constexpr const char* kUsageText =
    "usage:\n"
    "  redgem                            open the project hub (new / recent games)\n"
    "  redgem <game_dir>                 open the editor\n"
    "  redgem <game_dir> --play [--screenshots <dir>]   play the game\n"
    "  redgem new <game_dir> [--template empty|platformer]   create a game\n"
    "  redgem build <game_dir> <out_dir> [--target linux|windows|android]   make a standalone game (an executable, or "
    "an .apk)\n"
    "  redgem relay [--port <number>]    run a meeting point for multiplayer games over the internet\n"
    "\n"
    "  --gpu auto|dedicated|integrated      graphics card for this run (default: the saved setting,\n"
    "                                       else the dedicated card when there is one)\n"
    "\n";

// A command line being read: what was understood so far, and how far the reading got.
struct Reading {
  // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
  const std::vector<std::string>& args;
  std::size_t next = 0;  // the argument to read next
  CliResult result;
  bool mode_chosen = false;
  bool conflict = false;  // two different things were asked for
  bool done = false;      // an error or --help ended the reading
};

void fail(Reading& reading, const std::string& message) {
  reading.result.ok = false;
  reading.result.error = message;
  reading.done = true;
}

// What to do is said once: by a subcommand or by one of the mode flags.
void choose(Reading& reading, RunMode mode) {
  CliOptions& options = reading.result.options;
  reading.conflict = reading.conflict || (reading.mode_chosen && options.mode != mode);
  options.mode = mode;
  reading.mode_chosen = true;
}

bool is_option(const std::string& arg) { return !arg.empty() && arg[0] == kOptionMark; }

// What the option just read was given: the next argument, unless it is missing or is itself an option.
std::optional<std::string> value(Reading& reading) {
  if (reading.next >= reading.args.size() || is_option(reading.args[reading.next])) return std::nullopt;
  return reading.args[reading.next++];
}

bool is_port(const std::string& text) {
  return !text.empty() && text.find_first_not_of(kDigits) == std::string::npos && text.size() <= kPortDigits &&
         std::stoi(text) <= kLargestPort;
}

void read_help(Reading& reading) {
  reading.result.options = CliOptions{};
  reading.result.options.mode = RunMode::Help;  // help wins over anything else on the line
  reading.result.ok = true;
  reading.done = true;
}

void read_play(Reading& reading) { choose(reading, RunMode::Game); }

void read_screenshots(Reading& reading) {
  const auto folder = value(reading);
  if (!folder) {
    fail(reading, "--screenshots needs a directory");
    return;
  }
  reading.result.options.screenshot_dir = *folder;
}

void read_template(Reading& reading) {
  const auto name = value(reading);
  if (!name) {
    fail(reading, "--template needs a name");
    return;
  }
  reading.result.options.template_name = *name;
}

void read_target(Reading& reading) {
  const auto name = value(reading);
  if (!name || !build_target_from_name(*name)) {
    fail(reading, "--target needs linux, windows or android");
    return;
  }
  reading.result.options.build_target = *name;
}

void read_gpu(Reading& reading) {
  const auto name = value(reading);
  if (!name || !gpu_choice_from_name(*name)) {
    fail(reading, "--gpu needs auto, dedicated or integrated");
    return;
  }
  reading.result.options.gpu = *name;
}

void read_port(Reading& reading) {
  const auto number = value(reading);
  if (!number || !is_port(*number)) {
    fail(reading, "--port needs a number up to 65535");
    return;
  }
  reading.result.options.relay_port = std::stoi(*number);
}

struct Option {
  const char* name;
  void (*read)(Reading&);
};
constexpr Option kOptions[] = {
    {kHelpOption, read_help},         {kShortHelpOption, read_help},
    {kPlayOption, read_play},         {kScreenshotsOption, read_screenshots},
    {kTemplateOption, read_template}, {kTargetOption, read_target},
    {kGpuOption, read_gpu},           {kPortOption, read_port},
};

struct Command {
  const char* word;
  RunMode mode;
};
constexpr Command kCommands[] = {
    {kNewCommand, RunMode::NewGame}, {kBuildCommand, RunMode::BuildGame}, {kRelayCommand, RunMode::Relay}};

const Option* find_option(const std::string& arg) {
  const auto* found = std::find_if(std::begin(kOptions), std::end(kOptions),
                                   [&arg](const Option& option) { return arg == option.name; });
  return found != std::end(kOptions) ? found : nullptr;
}

// A subcommand is the first word of the line, or it is not there.
void read_command(Reading& reading) {
  if (reading.args.empty()) return;
  const auto* command = std::find_if(std::begin(kCommands), std::end(kCommands),
                                     [&reading](const Command& known) { return reading.args.front() == known.word; });
  if (command == std::end(kCommands)) return;
  choose(reading, command->mode);
  reading.next = 1;
}

// Where an argument that is no option goes: the game's folder, then (for a build) the folder to
// build into. nullptr when the line has no place for it.
std::filesystem::path* place_for_folder(CliOptions& options) {
  if (options.mode == RunMode::Relay) return nullptr;
  if (options.game_dir.empty()) return &options.game_dir;
  if (options.mode == RunMode::BuildGame && options.build_dir.empty()) return &options.build_dir;
  return nullptr;
}

void read_folder(Reading& reading, const std::string& arg) {
  std::filesystem::path* place = place_for_folder(reading.result.options);
  if (place == nullptr) {
    fail(reading, "unexpected argument " + arg);
    return;
  }
  *place = normalized_folder(arg);
}

void read_argument(Reading& reading) {
  const std::string& arg = reading.args[reading.next++];
  if (const Option* option = find_option(arg)) {
    option->read(reading);
  } else if (is_option(arg)) {
    fail(reading, "unknown option " + arg);
  } else {
    read_folder(reading, arg);
  }
}

bool needs_game(RunMode mode) {
  return mode == RunMode::Game || mode == RunMode::NewGame || mode == RunMode::BuildGame;
}

// What is wrong with a line that was read to its end; empty when nothing is.
std::string problem_with(const Reading& reading) {
  const CliOptions& options = reading.result.options;
  if (reading.conflict) return "choose one of: new, build, relay, --play";
  if (needs_game(options.mode) && options.game_dir.empty()) return "missing <game_dir>";
  if (options.mode == RunMode::BuildGame && options.build_dir.empty()) return "missing <out_dir>";
  return {};
}

}  // namespace

std::string usage() {
  return std::string(kUsageText) + "  <game_dir> holds " + config::kSceneFileName + " (exported from Blender) and a " +
         config::kScriptsDirName + "/ folder.\n";
}

EngineOptions developer_play_options(const CliOptions& options, GpuChoice gpu) {
  EngineOptions engine;
  engine.game_dir = options.game_dir;
  engine.screenshot_dir = options.screenshot_dir;
  engine.show_fps = true;
  engine.gpu = gpu;
  return engine;
}

CliResult parse_cli(const std::vector<std::string>& args) {
  Reading reading{args};
  reading.result.options.screenshot_dir = config::kDefaultScreenshotDir;
  read_command(reading);
  while (!reading.done && reading.next < args.size()) read_argument(reading);
  if (reading.done) return reading.result;
  reading.result.error = problem_with(reading);
  reading.result.ok = reading.result.error.empty();
  return reading.result;
}

}  // namespace redgem
