#include "server/server_command.hpp"

#include <algorithm>

#include "core/command_line.hpp"

namespace redgem::server {

namespace {

constexpr const char* kNoGame = "which game? redgem-server <game_dir>";
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
};

std::string usage_text() {
  const auto number = [](int value) { return std::to_string(value); };
  return std::string("usage:\n") +
         "  redgem-server <game_dir> [options]    run a game on a server, for players to join over the internet\n\n" +
         "  --port <number>           the port to listen on (default: " + number(kDefaultServerPort) +
         "; 0 picks a free one)\n" +
         "  --max-players <number>    players in one room (default: " + number(kDefaultMaxPlayers) + ", at most " +
         number(kMostPlayersPerRoom) + ")\n" +
         "  --rooms <number>          rooms open at once, each a match of its own (default: " +
         number(kDefaultMaxRooms) + ", at most " + number(kMostRooms) + ")\n" +
         "  --per-address <number>    machines one address may connect (default: " +
         number(net::kDefaultConnectionsPerAddress) + ", at most " + number(net::kMostConnectionsPerAddress) +
         "; players\n" + "                            behind one shared address, a campus, may need more)\n" +
         "  --data <dir>              where the game's Save is kept, for every room (default: a folder beside\n" +
         "                            the game's, named after it with " + kDataFolderSuffix + ")\n";
}

// Reads --data and the folder it is given.
std::string read_data_dir(const std::vector<std::string>& args, std::size_t& next, ServerSettings& settings) {
  ++next;
  const auto folder = command_line::option_value(args, next);
  if (!folder) return std::string(kDataOption) + " needs a folder";
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
  return settings.game_dir.empty() ? kNoGame : "";
}

}  // namespace

std::string server_usage() { return usage_text(); }

ServerCommand parse_server_command(const std::vector<std::string>& args) {
  ServerCommand command;
  command.help = std::any_of(args.begin(), args.end(), command_line::asks_for_help);
  if (!command.help) command.error = read_line(args, command.settings);
  command.ok = command.error.empty();
  if (command.settings.data_dir.empty()) command.settings.data_dir = data_dir_beside(command.settings.game_dir);
  return command;
}

}  // namespace redgem::server
