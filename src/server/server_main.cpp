// redgem-server: a game played on a server with nobody at it, for players to join over the
// internet (see server/game_server.hpp). It opens no window and needs no display.
//   redgem-server <game_dir> [--port <number>] [--max-players <number>] [--rooms <number>]

#include <exception>
#include <string>
#include <vector>

#include "app/game_texts.hpp"
#include "app/user_settings.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"
#include "server/server_program.hpp"
#include "server/server_texts.hpp"

int main(int argc, char** argv) {
  try {
    redgem::add_game_texts();
    redgem::server::add_server_texts();
    redgem::translations::use_language(redgem::language_for_run());
    return redgem::server::run_server_program(std::vector<std::string>(argv + 1, argv + argc));
  } catch (const std::exception& error) {
    // (Whatever goes wrong is said: nothing ends the program without a word.)
    redgem::log::error(error.what());
    return redgem::kExitFailure;
  }
}
