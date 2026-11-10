// redgem-player: what players receive. Building a game copies this program next to the game's
// files under the game's name; started, it plays the game in the folder beside it and does nothing
// else (it takes no arguments). It is made of the runtime alone: none of the tools games are made
// with are in it.

#include <exception>
#include <filesystem>
#include <new>

#include "app/bundled_game.hpp"
#include "app/crash_report.hpp"
#include "app/game_data_location.hpp"
#include "app/game_texts.hpp"
#include "app/user_settings.hpp"
#include "app/window_presenter.hpp"
#include "core/config.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"

namespace {

// The window of the player's own run: the one that keeps what the player chooses, in the players'
// folder of the game (see app/game_data_location.hpp), and reads nothing of RedGem's own.
redgem::GameWindowOptions players_window(const std::filesystem::path& game, const std::filesystem::path& data) {
  using namespace redgem;
  const GameDataFiles files = game_data_files(data);
  GameWindowOptions options;
  options.game_dir = game;
  options.data_folder = data;
  options.gpu = gpu_choice_for_run(std::nullopt, files.settings);
  const PlayerDisplay display = player_display(game, files.settings);
  options.display = display.display;
  options.display_kept_in = display.kept_in;
  return options;
}

// Plays the game beside `program`; an error when there is none. From the moment the game is
// found, a crash writes a report in the game's folder (see app/crash_report.hpp).
int play_game_beside(const std::filesystem::path& program) {
  using namespace redgem;
  const auto game = bundled_game(program);
  if (!game) {
    const std::filesystem::path expected = program.parent_path() / kBundledGameFolder / config::kSceneFileName;
    log::error("no game to play: " + expected.string() + " is missing");
    return kExitFailure;
  }
  const std::filesystem::path data = players_data_folder(*game);
  translations::use_language(language_for_run(game_data_files(data).settings));
  crash::install_crash_handling({*game, data, true});
  return play_in_window(players_window(*game, data));
}

}  // namespace

int main(int, char**) {
  try {
    redgem::add_game_texts();
    return play_game_beside(redgem::platform::executable_path());
  } catch (const std::bad_alloc&) {
    return redgem::crash::end_out_of_memory();
  } catch (const std::exception& error) {
    // (Whatever goes wrong while playing is said, and fails the run.)
    return redgem::crash::end_with_exception(error.what());
  }
}
