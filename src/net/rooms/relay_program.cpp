#include "net/rooms/relay_program.hpp"

#include <algorithm>
#include <iostream>

#include "core/clock.hpp"
#include "core/command_line.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/ports.hpp"
#include "core/translations.hpp"

namespace redgem::net {

namespace {

using NumberOption = command_line::NumberOption<RelayCommand>;

constexpr const char* kDataOption = "--data";

constexpr NumberOption kOptions[] = {
    // (Port 0 is no port to meet on: it stands for the usual one.)
    {"--port", 0, kLargestPort,
     [](RelayCommand& command, int port) {
       if (port != 0) command.port = static_cast<std::uint16_t>(port);
     }},
    {"--per-address", 1, kMostConnectionsPerAddress,
     [](RelayCommand& command, int machines) { command.limits.connections_per_address = machines; }},
    {"--rooms-per-address", 1, static_cast<int>(kMaxRelayRooms),
     [](RelayCommand& command, int rooms) { command.limits.rooms_per_address = rooms; }},
};

// A number as the usage writes it.
template <typename Number>
std::string number(Number value) {
  return std::to_string(value);
}

// The numbers the usage gives, by the names of its placeholders.
TextValues usage_values() {
  return {{"default_port", number(kDefaultRelayPort)},
          {"default_per_address", number(kDefaultConnectionsPerAddress)},
          {"most_per_address", number(kMostConnectionsPerAddress)},
          {"default_rooms", number(kDefaultRoomsPerAddress)},
          {"most_rooms", number(kMaxRelayRooms)},
          {"default_data", kDefaultRelayDataDir}};
}

// Reads --data and the folder it is given.
std::string read_data_dir(const std::vector<std::string>& args, std::size_t& next, RelayCommand& command) {
  ++next;
  const auto folder = command_line::option_value(args, next);
  if (!folder) return tr("relay.cli.data_needs_folder");
  command.data_dir = *folder;
  return {};
}

// Reads --fingerprint, which is given nothing.
std::string read_fingerprint(std::size_t& next, RelayCommand& command) {
  command.show_fingerprint = true;
  ++next;
  return {};
}

// Reads the word at `next` (and what it is given); an error when it makes no sense.
std::string read_word(const std::vector<std::string>& args, std::size_t& next, RelayCommand& command) {
  if (args[next] == kDataOption) return read_data_dir(args, next, command);
  if (args[next] == command_line::kFingerprintOption) return read_fingerprint(next, command);
  return command_line::read_number_option(kOptions, args, next, command);
}

// Reads the whole line into `command`; what makes no sense in it, if anything does.
std::string read_line(const std::vector<std::string>& args, RelayCommand& command) {
  for (std::size_t next = 0; next < args.size();) {
    std::string error = read_word(args, next, command);
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
  const Clock& clock = steady_clock();
  std::size_t rooms = 0;
  double before = clock.now();
  for (;;) {
    const double now = clock.now();
    serve_relay_once(relay, static_cast<float>(now - before));
    before = now;
    report_rooms(relay, rooms);
  }
}

// Starts the relay, proving itself with the identity kept in its data folder; false, with the
// reason logged, when it cannot.
bool start_relay(RelayServer& relay, const RelayCommand& command) {
  std::string error;
  const auto identity = TlsIdentity::kept_in(command.data_dir, error);
  if (identity == nullptr) {
    log::error("cannot take the relay's TLS identity: " + error);
    return false;
  }
  if (!relay.start(command.port, error, identity)) {
    log::error("cannot listen on port " + std::to_string(command.port) + ": " + error);
    return false;
  }
  log::info("relay listening on port " + std::to_string(relay.port()) + " (Ctrl+C stops it)");
  log::info(fingerprint_notice(identity->fingerprint()));
  return true;
}

int run_relay(const RelayCommand& command) {
  RelayServer relay(command.limits);
  if (!start_relay(relay, command)) return kExitFailure;
  serve_until_stopped(relay);
}

int show_fingerprint(const RelayCommand& command) {
  return print_fingerprint(command.data_dir, std::cout) ? kExitSuccess : kExitFailure;
}

}  // namespace

std::string relay_usage() { return tr("relay.cli.usage", usage_values()); }

RelayCommand parse_relay_command(const std::vector<std::string>& args) {
  RelayCommand command;
  command.help = std::any_of(args.begin(), args.end(), command_line::asks_for_help);
  if (!command.help) command.error = read_line(args, command);
  command.ok = command.error.empty();
  return command;
}

bool serve_relay_once(RelayServer& relay, float seconds) {
  relay.poll(seconds);
  return relay.wait(kRelayWakeSeconds);
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
  if (command.show_fingerprint) return show_fingerprint(command);
  return run_relay(command);
}

}  // namespace redgem::net
