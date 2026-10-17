#pragma once

#include <string>
#include <vector>

#include "server/game_server.hpp"

namespace redgem::server {

// What the redgem-server command line asks for.
struct ServerCommand {
  bool ok = false;
  std::string error;  // why not, when it is not ok
  bool help = false;
  ServerSettings settings;
};

// Reads `redgem-server <game_dir> [--port <number>] [--max-players <number>] [--rooms <number>]
// [--per-address <number>] [--threads <number>] [--data <dir>]` (and --help, which wins over the
// rest of the line).
ServerCommand parse_server_command(const std::vector<std::string>& args);
std::string server_usage();

}  // namespace redgem::server
