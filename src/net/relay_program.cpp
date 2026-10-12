#include "net/relay_program.hpp"

#include <chrono>
#include <iostream>

#include "core/exit_codes.hpp"
#include "core/log.hpp"

namespace redgem::net {

namespace {

constexpr std::size_t kPortDigits = 5;
constexpr int kLargestPort = 65535;
constexpr const char* kDigits = "0123456789";
constexpr const char* kHelpOption = "--help";
constexpr const char* kShortHelpOption = "-h";
constexpr const char* kPortOption = "--port";
constexpr std::size_t kPortWords = 2;  // the option and its number

constexpr const char* kUsageText =
    "usage:\n"
    "  redgem-relay [--port <number>]    run a meeting point for multiplayer games over the internet\n"
    "\n"
    "  --port <number>    the port to listen on (default: ";

bool is_port(const std::string& text) {
  return !text.empty() && text.find_first_not_of(kDigits) == std::string::npos && text.size() <= kPortDigits &&
         std::stoi(text) <= kLargestPort;
}

RelayCommand refused(const std::string& why) {
  RelayCommand command;
  command.error = why;
  return command;
}

bool asks_for_help(const std::vector<std::string>& args) {
  for (const std::string& arg : args) {
    if (arg == kHelpOption || arg == kShortHelpOption) return true;
  }
  return false;
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

int run_relay(std::uint16_t port) {
  RelayServer relay;
  std::string error;
  if (!relay.start(port, error)) {
    log::error("cannot listen on port " + std::to_string(port) + ": " + error);
    return kExitFailure;
  }
  log::info("relay listening on port " + std::to_string(relay.port()) + " (Ctrl+C stops it)");
  serve_until_stopped(relay);
}

}  // namespace

std::string relay_usage() { return kUsageText + std::to_string(kDefaultRelayPort) + ")\n"; }

RelayCommand parse_relay_command(const std::vector<std::string>& args) {
  RelayCommand command;
  command.ok = true;
  command.help = asks_for_help(args);
  if (command.help || args.empty()) return command;
  if (args.front() != kPortOption) return refused("unexpected argument " + args.front());
  // (One option, said once: anything after its value has no place on the line.)
  if (args.size() != kPortWords || !is_port(args.back())) return refused("--port needs a number up to 65535");
  // (Port 0 is no port to meet on: it stands for the usual one.)
  const int port = std::stoi(args.back());
  if (port != 0) command.port = static_cast<std::uint16_t>(port);
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
  return run_relay(command.port);
}

}  // namespace redgem::net
