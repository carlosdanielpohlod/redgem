#pragma once

#include <functional>
#include <string>
#include <vector>

#include "server/game_server.hpp"

// The redgem-server program: a dedicated server (see GameServer) run from a command line. It
// plays the game with no window, so it runs on machines with no display and no graphics card.
namespace redgem::server {

// Runs the server's turns on the real clock, one every fixed step of the engine, until `stop`
// says so (it is asked between turns). Says in the log how many rooms and players there are
// whenever that changes.
void serve_until(GameServer& server, const std::function<bool()>& stop);

// The whole program: reads the command line and, unless it asks for help or makes no sense, runs
// the server until the process is asked to stop (Ctrl+C, SIGTERM). Returns the exit code.
int run_server_program(const std::vector<std::string>& args);

}  // namespace redgem::server
