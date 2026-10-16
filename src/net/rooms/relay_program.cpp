#include "net/rooms/relay_program.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

#include "core/command_line.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"

namespace redgem::net {

namespace {

using NumberOption = command_line::NumberOption<RelayCommand>;

constexpr NumberOption kOptions[] = {
    // (Port 0 is no port to meet on: it stands for the usual one.)
    {"--port", 0, command_line::kLargestPort,
     [](RelayCommand& command, int port) {
       if (port != 0) command.port = static_cast<std::uint16_t>(port);
     }},
    {"--per-address", 1, kMostConnectionsPerAddress,
     [](RelayCommand& command, int machines) { command.limits.connections_per_address = machines; }},
    {"--rooms-per-address", 1, static_cast<int>(kMaxRelayRooms),
     [](RelayCommand& command, int rooms) { command.limits.rooms_per_address = rooms; }},
};

std::string usage_text() {
  const auto number = [](auto value) { return std::to_string(value); };
  return std::string("usage:\n") +
         "  redgem-relay [--port <number>] [--per-address <number>] [--rooms-per-address <number>]\n" +
         "      run a meeting point for multiplayer games over the internet\n\n" +
         "  --port <number>                the port to listen on (default: " + number(kDefaultRelayPort) + ")\n" +
         "  --per-address <number>         machines one address may connect (default: " +
         number(kDefaultConnectionsPerAddress) + ", at most " + number(kMostConnectionsPerAddress) + ")\n" +
         "  --rooms-per-address <number>   rooms one address may have open (default: " +
         number(kDefaultRoomsPerAddress) + ", at most " + number(kMaxRelayRooms) + ")\n" +
         "Players behind one shared address (a campus, some mobile networks) may need more of both.\n";
}

// Reads the whole line into `command`; what makes no sense in it, if anything does.
std::string read_line(const std::vector<std::string>& args, RelayCommand& command) {
  for (std::size_t next = 0; next < args.size();) {
    std::string error = command_line::read_number_option(kOptions, args, next, command);
    if (!error.empty()) return error;
  }
  return {};
}

void report_rooms(const RelayServer& relay, std::size_t& rooms) {
  if (relay.room_count() == rooms) return;
  rooms = relay.room_count();
  log::info(std::to_string(rooms) + " room(s) open, " + std::to_string(relay.connection_count()) + " machine(s)");
}

[[noreturn]] void serve_until_stopped(RelayServer& relay) {
  std::size_t rooms = 0;
  auto before = std::chrono::steady_clock::now();
  for (;;) {
    const auto now = std::chrono::steady_clock::now();
    serve_relay_once(relay, std::chrono::duration<float>(now - before).count());
    before = now;
    report_rooms(relay, rooms);
  }
}

int run_relay(const RelayCommand& command) {
  RelayServer relay(command.limits);
  std::string error;
  if (!relay.start(command.port, error)) {
    log::error("cannot listen on port " + std::to_string(command.port) + ": " + error);
    return kExitFailure;
  }
  log::info("relay listening on port " + std::to_string(relay.port()) + " (Ctrl+C stops it)");
  serve_until_stopped(relay);
}

}  // namespace

std::string relay_usage() { return usage_text(); }

RelayCommand parse_relay_command(const std::vector<std::string>& args) {
  RelayCommand command;
  command.help = std::any_of(args.begin(), args.end(), command_line::asks_for_help);
  if (!command.help) command.error = read_line(args, command);
  command.ok = command.error.empty();
  return command;
}

void serve_relay_once(RelayServer& relay, float seconds) {
  relay.poll(seconds);
  relay.wait(kRelayWakeSeconds);
}

int run_relay_program(const std::vector<std::string>& args) {
  const RelayCommand command = parse_relay_command(args);
  if (!command.ok) {
    std::cerr << command.error << "\n\n" << relay_usage();
    return kExitUsage;
  }
  if (command.help) {
    std::cout << relay_usage();
    return kExitSuccess;
  }
  return run_relay(command);
}

}  // namespace redgem::net
