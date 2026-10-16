#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/rooms/relay_server.hpp"

// The redgem-relay program: a relay (see RelayServer) run from a command line, on a server.
// Nothing here opens a window or runs a script, so the program is made of this library alone.
namespace redgem::net {

// The longest a relay with nothing arriving sleeps before it looks at its clocks again: silent
// machines are let go, and quiet ones asked whether they are still there, about this late at most.
inline constexpr float kRelayWakeSeconds = 1.0f;

// What a relay's command line asks for.
struct RelayCommand {
  bool ok = false;
  std::string error;  // why not, when it is not ok
  bool help = false;
  std::uint16_t port = kDefaultRelayPort;
};

// Reads `redgem-relay [--port <number>]` (and --help, which wins over the rest of the line).
RelayCommand parse_relay_command(const std::vector<std::string>& args);
std::string relay_usage();

// One turn of a relay that has nothing else to do: sleeps until something arrives (see
// kRelayWakeSeconds), then moves the messages. `seconds` is the time since the turn before.
void serve_relay_once(RelayServer& relay, float seconds);

// The whole program: reads the command line and, unless it asks for help or makes no sense, runs
// a relay until the process is stopped. Returns the exit code.
int run_relay_program(const std::vector<std::string>& args);

}  // namespace redgem::net
