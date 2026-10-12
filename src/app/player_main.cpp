// redgem-player: what players receive. Building a game copies this program next to the game's
// files under the game's name; started, it plays the game in the folder beside it and does nothing
// else (it takes no arguments). It is made of the runtime alone: none of the tools games are made
// with are in it.

#include <exception>
#include <filesystem>

#include "app/bundled_game.hpp"
#include "app/user_settings.hpp"
#include "app/window_presenter.hpp"
#include "core/config.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"

namespace {

// Plays the game beside `program`; an error when there is none.
int play_game_beside(const std::filesystem::path& program) {
  using namespace redgem;
  const auto game = bundled_game(program);
  if (!game) {
    const std::filesystem::path expected = program.parent_path() / kBundledGameFolder / config::kSceneFileName;
    log::error("no game to play: " + expected.string() + " is missing");
    return kExitFailure;
  }
  GameWindowOptions options;
  options.game_dir = *game;
  options.gpu = gpu_choice_for_run(std::nullopt);
  return play_in_window(options);
}

}  // namespace

int main(int, char**) {
  try {
    return play_game_beside(redgem::platform::executable_path());
  } catch (const std::exception& error) {
    // (Whatever goes wrong while playing is said, and fails the run.)
    redgem::log::error(error.what());
    return redgem::kExitFailure;
  }
}
