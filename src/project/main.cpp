// redgem: the program games are made with. It opens the editor and the project hub and does what
// the command line asks (new, build, --play). What players receive is another program,
// redgem-player, which a build copies next to the game's files; where games meet over the
// internet is a third, redgem-relay, and a game run on a server for players to join a fourth,
// redgem-server.

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "app/game_texts.hpp"
#include "app/user_settings.hpp"
#include "app/window_presenter.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/editor_texts.hpp"
#include "project/cli.hpp"
#include "project/editor_run.hpp"
#include "project/game_build.hpp"
#include "project/game_templates.hpp"
#include "project/project_texts.hpp"

namespace {

int run_editor(const redgem::CliOptions& options) {
  // (The card is chosen once, before the first window: the hub and every editor opened from it use the same.)
  return redgem::run_editor(options.game_dir, redgem::gpu_choice_for_run(options.gpu));
}

int play_game(const redgem::CliOptions& options) {
  return redgem::play_in_window(redgem::developer_play_options(options, redgem::gpu_choice_for_run(options.gpu)));
}

int show_help(const redgem::CliOptions&) {
  std::cout << redgem::usage();
  return redgem::kExitSuccess;
}

int new_game(const redgem::CliOptions& options) {
  using namespace redgem;
  std::string error;
  if (!create_game(options.game_dir, options.template_name, error)) {
    log::error(error);
    return kExitFailure;
  }
  std::cout << tr("project.cli.created", {{"game_dir", options.game_dir.string()}, {"template", options.template_name}})
            << "\n";
  return kExitSuccess;
}

// The system a build is for: the one named on the command line (parse_cli took only names that
// exist), else the one this program runs on.
redgem::BuildTarget target_of(const redgem::CliOptions& options) {
  const auto named = redgem::build_target_from_name(options.build_target);
  return named ? *named : redgem::host_build_target();
}

int build(const redgem::CliOptions& options) {
  using namespace redgem;
  std::string error;
  const BuildTarget target = target_of(options);
  const auto executable = build_game_for(options.game_dir, options.build_dir, target, error);
  if (!executable) {
    log::error(error);
    return kExitFailure;
  }
  // (An Android app is not run from here: it is installed on a phone.)
  const char* said = target == BuildTarget::Android ? "project.cli.built_for_phone" : "project.cli.built";
  std::cout << tr(said, {{"out_dir", options.build_dir.string()},
                         {"target", build_target_name(target)},
                         {"executable", executable->string()}})
            << "\n";
  return kExitSuccess;
}

// What each way of running the program does.
struct Mode {
  redgem::RunMode mode;
  int (*run)(const redgem::CliOptions&);
};
constexpr Mode kModes[] = {
    {redgem::RunMode::Help, show_help},    {redgem::RunMode::NewGame, new_game}, {redgem::RunMode::BuildGame, build},
    {redgem::RunMode::Editor, run_editor}, {redgem::RunMode::Game, play_game},
};

int run_mode(const redgem::CliOptions& options) {
  for (const Mode& known : kModes) {
    if (known.mode == options.mode) return known.run(options);
  }
  return redgem::kExitFailure;
}

// Whatever goes wrong while running is said, and fails the run.
template <typename Run>
int reporting_errors(Run run) {
  try {
    return run();
  } catch (const std::exception& error) {
    redgem::log::error(error.what());
    return redgem::kExitFailure;
  }
}

// The texts redgem shows, its own and those of the games it plays, in its user's language.
void set_up_texts() {
  using namespace redgem;
  add_game_texts();
  add_editor_texts();
  add_project_texts();
  translations::use_language(language_for_run());
}

// What the command line asks for, done. (Reading the line is inside the same net as the running:
// nothing a user types ends the program without a word.)
int run_command_line(const std::vector<std::string>& args) {
  using namespace redgem;
  set_up_texts();
  const CliResult cli = parse_cli(args);
  if (!cli.ok) {
    std::cerr << cli.error << "\n\n" << usage();
    return kExitUsage;
  }
  return run_mode(cli.options);
}

}  // namespace

int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  return reporting_errors([&args] { return run_command_line(args); });
}
