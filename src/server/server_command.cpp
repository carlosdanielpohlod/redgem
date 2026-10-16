#include "server/server_command.hpp"

#include <algorithm>

#include "core/command_line.hpp"
#include "core/translations.hpp"

namespace redgem::server {

namespace {

constexpr const char* kDataOption = "--data";

using NumberOption = command_line::NumberOption<ServerSettings>;

constexpr NumberOption kOptions[] = {
    {"--port", 0, command_line::kLargestPort,
     [](ServerSettings& settings, int port) { settings.port = static_cast<std::uint16_t>(port); }},
    {"--max-players", 1, kMostPlayersPerRoom,
     [](ServerSettings& settings, int players) { settings.limits.max_players = players; }},
    {"--rooms", 1, kMostRooms, [](ServerSettings& settings, int rooms) { settings.limits.max_rooms = rooms; }},
    {"--per-address", 1, net::kMostConnectionsPerAddress,
     [](ServerSettings& settings, int machines) { settings.connections_per_address = machines; }},
    {"--threads", 1, kMostThreads, [](ServerSettings& settings, int threads) { settings.threads = threads; }},
};

std::string number(int value) { return std::to_string(value); }

// The numbers and names the usage gives, by the names of its placeholders.
TextValues usage_values() {
  return {{"default_port", number(kDefaultServerPort)},
          {"default_per_address", number(net::kDefaultConnectionsPerAddress)},
          {"most_per_address", number(net::kMostConnectionsPerAddress)},
          {"data_suffix", kDataFolderSuffix},
          {"default_players", number(kDefaultMaxPlayers)},
          {"most_players", number(kMostPlayersPerRoom)},
          {"default_rooms", number(kDefaultMaxRooms)},
          {"most_rooms", number(kMostRooms)},
          {"cores", number(processor_count())},
          {"most_threads", number(kMostThreads)}};
}

// Reads --data and the folder it is given.
std::string read_data_dir(const std::vector<std::string>& args, std::size_t& next, ServerSettings& settings) {
  ++next;
  const auto folder = command_line::option_value(args, next);
  if (!folder) return tr("server.cli.data_needs_folder");
  settings.data_dir = *folder;
  return {};
}

// Reads the word at `next` (and an option's number); an error when it makes no sense.
std::string read_word(const std::vector<std::string>& args, std::size_t& next, ServerSettings& settings) {
  if (args[next] == kDataOption) return read_data_dir(args, next, settings);
  if (command_line::is_option(args[next])) return command_line::read_number_option(kOptions, args, next, settings);
  if (!settings.game_dir.empty()) return command_line::unexpected(args[next]);
  settings.game_dir = args[next++];
  return {};
}

// Reads the whole line into `settings`; what makes no sense in it, if anything does.
std::string read_line(const std::vector<std::string>& args, ServerSettings& settings) {
  for (std::size_t next = 0; next < args.size();) {
    std::string error = read_word(args, next, settings);
    if (!error.empty()) return error;
  }
  return settings.game_dir.empty() ? tr("server.cli.no_game") : "";
}

}  // namespace

std::string server_usage() { return tr("server.cli.usage", usage_values()); }

ServerCommand parse_server_command(const std::vector<std::string>& args) {
  ServerCommand command;
  command.help = std::any_of(args.begin(), args.end(), command_line::asks_for_help);
  if (!command.help) command.error = read_line(args, command.settings);
  command.ok = command.error.empty();
  if (command.settings.data_dir.empty()) command.settings.data_dir = data_dir_beside(command.settings.game_dir);
  return command;
}

}  // namespace redgem::server
