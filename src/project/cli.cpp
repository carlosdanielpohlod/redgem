#include "project/cli.hpp"

#include <algorithm>
#include <iterator>

#include "core/command_line.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "project/cli_reading.hpp"
#include "project/game_templates.hpp"

namespace redgem {

namespace cli {

void fail(Reading& reading, const std::string& message) {
  reading.result.ok = false;
  reading.result.error = message;
  reading.done = true;
}

void choose(Reading& reading, RunMode mode) {
  CliOptions& options = reading.result.options;
  reading.conflict = reading.conflict || (reading.mode_chosen && options.mode != mode);
  options.mode = mode;
  reading.mode_chosen = true;
}

}  // namespace cli

namespace {

using cli::Reading;

// The subcommands.
constexpr const char* kNewCommand = "new";
constexpr const char* kBuildCommand = "build";

// The usage, around the one line that lists the templates there are.
constexpr const char* kUsageStart =
    "usage:\n"
    "  redgem                            open the project hub (new / recent games)\n"
    "  redgem <game_dir>                 open the editor\n"
    "  redgem <game_dir> --play          play the game\n"
    "  redgem new <game_dir> [--template ";
constexpr const char* kTemplateSeparator = "|";
constexpr const char* kUsageEnd =
    "]   create a game\n"
    "  redgem build <game_dir> <out_dir> [--target linux|windows|macos|android]   make a standalone game (an executable, or "
    "an .apk)\n"
    "\n"
    "  --gpu auto|dedicated|integrated      graphics card for this run (default: the saved setting,\n"
    "                                       else the dedicated card when there is one)\n"
    "\n"
    "  Where multiplayer games meet over the internet is a program of its own: redgem-relay.\n"
    "\n";

struct Command {
  const char* word;
  RunMode mode;
};
constexpr Command kCommands[] = {{kNewCommand, RunMode::NewGame}, {kBuildCommand, RunMode::BuildGame}};

// A subcommand is the first word of the line, or it is not there.
void read_command(Reading& reading) {
  if (reading.args.empty()) return;
  const auto* command = std::find_if(std::begin(kCommands), std::end(kCommands),
                                     [&reading](const Command& known) { return reading.args.front() == known.word; });
  if (command == std::end(kCommands)) return;
  cli::choose(reading, command->mode);
  reading.next = 1;
}

bool needs_game(RunMode mode) {
  return mode == RunMode::Game || mode == RunMode::NewGame || mode == RunMode::BuildGame;
}

bool lacks_game(const CliOptions& options) { return needs_game(options.mode) && options.game_dir.empty(); }

// Whether a build was asked for and the folder to build into was not given yet.
bool lacks_out_dir(const CliOptions& options) {
  return options.mode == RunMode::BuildGame && options.build_dir.empty();
}

// Where an argument that is no option goes: the game's folder, then (for a build) the folder to
// build into. nullptr when the line has no place for it.
std::filesystem::path* place_for_folder(CliOptions& options) {
  if (options.game_dir.empty()) return &options.game_dir;
  if (lacks_out_dir(options)) return &options.build_dir;
  return nullptr;
}

void read_folder(Reading& reading, const std::string& arg) {
  std::filesystem::path* place = place_for_folder(reading.result.options);
  if (place == nullptr) {
    cli::fail(reading, "unexpected argument " + arg);
    return;
  }
  *place = normalized_folder(arg);
}

void read_argument(Reading& reading) {
  const std::string& arg = reading.args[reading.next++];
  if (const cli::Option* option = cli::find_option(arg)) {
    option->read(reading);
  } else if (command_line::is_option(arg)) {
    cli::fail(reading, "unknown option " + arg);
  } else {
    read_folder(reading, arg);
  }
}

// What is wrong with a line that was read to its end; empty when nothing is.
std::string problem_with(const Reading& reading) {
  const CliOptions& options = reading.result.options;
  if (reading.conflict) return "choose one of: new, build, --play";
  if (lacks_game(options)) return "missing <game_dir>";
  if (lacks_out_dir(options)) return "missing <out_dir>";
  return {};
}

}  // namespace

std::string usage() {
  return kUsageStart + template_names(kTemplateSeparator) + kUsageEnd + "  <game_dir> holds " + config::kSceneFileName +
         " (exported from Blender) and a " + config::kScriptsDirName + "/ folder.\n";
}

GameWindowOptions developer_play_options(const CliOptions& options, GpuChoice gpu) {
  GameWindowOptions window;
  window.game_dir = options.game_dir;
  window.show_fps = true;
  window.gpu = gpu;
  return window;
}

CliResult parse_cli(const std::vector<std::string>& args) {
  Reading reading(args);
  read_command(reading);
  while (reading.more()) read_argument(reading);
  if (reading.done) return reading.result;
  reading.result.error = problem_with(reading);
  reading.result.ok = reading.result.error.empty();
  return reading.result;
}

}  // namespace redgem
