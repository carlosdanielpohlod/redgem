// redgem-relay: where games played over the internet meet (see net/rooms/relay_server.hpp). A program
// for servers: it opens no window and plays no game, only passes messages on.
//   redgem-relay [--port <number>]

#include <exception>
#include <string>
#include <vector>

#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "net/rooms/relay_program.hpp"

int main(int argc, char** argv) {
  try {
    return redgem::net::run_relay_program(std::vector<std::string>(argv + 1, argv + argc));
  } catch (const std::exception& error) {
    // (Whatever goes wrong is said: nothing ends the program without a word.)
    redgem::log::error(error.what());
    return redgem::kExitFailure;
  }
}
