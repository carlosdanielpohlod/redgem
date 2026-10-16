#include "net/rooms/relay_program.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <iterator>
#include <optional>

#include "core/command_line.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"

namespace redgem::net {

namespace {

constexpr std::size_t kMostDigits = 5;  // of any number the line takes
constexpr int kLargestPort = 65535;
constexpr const char* kDigits = "0123456789";

// An option of the line that is given a whole number, from `least` to `most`.
struct NumberOption {
  const char* name;
  int least;
  int most;
  void (*set)(RelayCommand&, int);
};

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

bool is_number(const std::string& text) {
  return !text.empty() && text.size() <= kMostDigits && text.find_first_not_of(kDigits) == std::string::npos;
}

// The number a word says, when it is one from `least` to `most`.
std::optional<int> number_between(const std::string& text, int least, int most) {
  if (!is_number(text)) return std::nullopt;
  const int number = std::stoi(text);
  if (number < least || number > most) return std::nullopt;
  return number;
}

const NumberOption* find_option(const std::string& word) {
  const auto* found = std::find_if(std::begin(kOptions), std::end(kOptions),
                                   [&word](const NumberOption& option) { return word == option.name; });
  return found != std::end(kOptions) ? found : nullptr;
}

// What an option says when it is not given a number it takes.
std::string refusal(const NumberOption& option) {
  const std::string range = option.least == 0
                                ? "up to " + std::to_string(option.most)
                                : "from " + std::to_string(option.least) + " to " + std::to_string(option.most);
  return std::string(option.name) + " needs a number " + range;
}

// Reads the option at `next` and its number; an error when it is no option, or not given a number it takes.
std::string read_option(const std::vector<std::string>& args, std::size_t& next, RelayCommand& command) {
  const NumberOption* option = find_option(args[next]);
  if (option == nullptr) return "unexpected argument " + args[next];
  ++next;
  const auto number = number_between(command_line::option_value(args, next).value_or(""), option->least, option->most);
  if (!number) return refusal(*option);
  option->set(command, *number);
  return {};
}

// Reads the whole line into `command`; what makes no sense in it, if anything does.
std::string read_line(const std::vector<std::string>& args, RelayCommand& command) {
  for (std::size_t next = 0; next < args.size();) {
    std::string error = read_option(args, next, command);
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
