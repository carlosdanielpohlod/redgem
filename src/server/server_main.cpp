// redgem-server: a game played on a server with nobody at it, for players to join over the
// internet (see server/game_server.hpp). It opens no window and needs no display.
//   redgem-server <game_dir> [--port <number>] [--max-players <number>] [--rooms <number>]

#include <exception>
#include <string>
#include <vector>

#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "server/server_program.hpp"

int main(int argc, char** argv) {
  try {
    return redgem::server::run_server_program(std::vector<std::string>(argv + 1, argv + argc));
  } catch (const std::exception& error) {
    // (Whatever goes wrong is said: nothing ends the program without a word.)
    redgem::log::error(error.what());
    return redgem::kExitFailure;
  }
}
