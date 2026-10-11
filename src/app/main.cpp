#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/exit_codes.hpp"
#include "app/cli.hpp"
#include "app/engine.hpp"
#include "app/game_project.hpp"
#include "app/user_settings.hpp"
#include "editor/editor_app.hpp"
#include "editor/project_hub.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"

namespace {

constexpr int kExitUsage = 2;

// The editor, with the project hub when no game is given (and for File > New / Open Game).
int run_editor(std::filesystem::path game) {
  using namespace redgem;
  bool started_with_game = !game.empty();  // a broken game from the command line exits; from the hub it returns there
  std::string hub_message;                 // why the last game did not open, shown in the hub
  while (true) {
    if (game.empty()) {
      ProjectHub hub({default_games_folder(), recent_games_file()});
      hub.show_error(hub_message);
      hub_message.clear();
      const auto chosen = hub.run();
      if (!chosen) return kExitSuccess;
      game = *chosen;
    } else {
      remember_game(recent_games_file(), game);
    }
    auto created = EditorApp::create(game);
    if (auto* error = std::get_if<std::string>(&created)) {
      log::error(*error);
      if (started_with_game) return kExitFailure;
      hub_message = "Cannot open " + game.filename().string() + ": " + *error;
      game.clear();  // back to the hub
      continue;
    }
    started_with_game = false;
    EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
    const int code = app.run();
    if (app.exit_action() != EditorExit::OpenHub) return code;
    game.clear();
  }
}

int play(const redgem::EngineOptions& options) {
  using namespace redgem;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) {
    log::error(*error);
    return kExitFailure;
  }
  return std::get<std::unique_ptr<Engine>>(created)->run();
}

}  // namespace

int main(int argc, char** argv) {
  using namespace redgem;
  const std::vector<std::string> args(argv + 1, argv + argc);
  // A built game: started by itself, the executable plays the game that ships next to it.
  if (args.empty()) {
    if (const auto game = bundled_game(platform::executable_path())) {
      apply_gpu_choice(std::nullopt);
      try {
        EngineOptions options;
        options.game_dir = *game;
        return play(options);
      } catch (const std::exception& error) {
        log::error(error.what());
        return kExitFailure;
      }
    }
  }
  const CliResult cli = parse_cli(args);
  if (!cli.ok) {
    std::cerr << cli.error << "\n\n" << usage();
    return kExitUsage;
  }
  const CliOptions& options = cli.options;
  apply_gpu_choice(options.gpu);
  try {
    switch (options.mode) {
      case RunMode::Help:
        std::cout << usage();
        return kExitSuccess;
      case RunMode::RubyTests:
        return run_ruby_tests(options.ruby_test_file);
      case RunMode::NewGame: {
        std::string error;
        if (!create_game(options.game_dir, options.template_name, error)) {
          log::error(error);
          return kExitFailure;
        }
        std::cout << "created " << options.game_dir.string() << " (" << options.template_name
                  << "). Open it with: redgem " << options.game_dir.string() << "\n";
        return kExitSuccess;
      }
      case RunMode::BuildGame: {
        std::string error;
        const BuildTarget target =
            options.build_target.empty() ? host_build_target() : *build_target_from_name(options.build_target);
        const auto executable = build_game_for(options.game_dir, options.build_dir, target, error);
        if (!executable) {
          log::error(error);
          return kExitFailure;
        }
        // (An Android app is not run from here: it is installed on a phone.)
        const char* next = target == BuildTarget::Android ? ": install it with adb install " : ": run ";
        std::cout << "built " << options.build_dir.string() << " for " << build_target_name(target) << next
                  << executable->string() << "\n";
        return kExitSuccess;
      }
      case RunMode::Editor:
        return run_editor(options.game_dir);
      case RunMode::Game:
        return play({options.game_dir, options.headless, options.test_script, options.screenshot_dir});
    }
  } catch (const std::exception& error) {
    log::error(error.what());
  }
  return kExitFailure;
}
