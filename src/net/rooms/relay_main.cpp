// redgem-relay: where games played over the internet meet (see net/rooms/relay_server.hpp). A program
// for servers: it opens no window and plays no game, only passes messages on.
//   redgem-relay [--port <number>]

#include <exception>
#include <string>
#include <vector>

#include "core/core_texts.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "net/rooms/relay_program.hpp"
#include "net/rooms/relay_texts.hpp"

int main(int argc, char** argv) {
  try {
    // (The relay has no user settings of its own: it speaks the system's language.)
    redgem::add_core_texts();
    redgem::net::add_relay_texts();
    redgem::translations::use_language(redgem::platform::system_language());
    return redgem::net::run_relay_program(std::vector<std::string>(argv + 1, argv + argc));
  } catch (const std::exception& error) {
    // (Whatever goes wrong is said: nothing ends the program without a word.)
    redgem::log::error(error.what());
    return redgem::kExitFailure;
  }
}
