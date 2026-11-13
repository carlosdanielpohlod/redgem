#include "server/server_command.hpp"

#include <algorithm>
#include <iterator>

#include "core/command_line.hpp"
#include "core/ports.hpp"
#include "core/translations.hpp"

namespace redgem::server {

namespace {

using NumberOption = command_line::NumberOption<ServerSettings>;

// An option of the line given a word (a folder, an address), what it says when it is given none,
// and what it sets.
struct TextOption {
  const char* name;
  const char* missing;  // the text's key
  void (*set)(ServerSettings&, const std::string&);
};

constexpr TextOption kTextOptions[] = {
    {"--data", "server.cli.data_needs_folder",
     [](ServerSettings& settings, const std::string& folder) { settings.data_dir = folder; }},
    {"--status-bind", "server.cli.status_bind_needs_address",
     [](ServerSettings& settings, const std::string& address) { settings.status.address = address; }},
};

constexpr NumberOption kOptions[] = {
    {"--port", 0, kLargestPort,
     [](ServerSettings& settings, int port) { settings.port = static_cast<std::uint16_t>(port); }},
    {"--max-players", 1, kMostPlayersPerRoom,
     [](ServerSettings& settings, int players) { settings.limits.max_players = players; }},
    {"--rooms", 1, kMostRooms, [](ServerSettings& settings, int rooms) { settings.limits.max_rooms = rooms; }},
    {"--per-address", 1, net::kMostConnectionsPerAddress,
     [](ServerSettings& settings, int machines) { settings.connections_per_address = machines; }},
    {"--threads", 1, kMostThreads, [](ServerSettings& settings, int threads) { settings.threads = threads; }},
    {"--status-port", 0, command_line::kLargestPort,
     [](ServerSettings& settings, int port) { settings.status.port = static_cast<std::uint16_t>(port); }},
    {"--status-log", 1, kMostStatusLogSeconds,
     [](ServerSettings& settings, int seconds) { settings.status.log_every_seconds = seconds; }},
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
          {"most_threads", number(kMostThreads)},
          {"status_address", kDefaultStatusAddress},
          {"most_status_log", number(kMostStatusLogSeconds)}};
}

// The option of kTextOptions named `word`; nullptr when it is none of them.
const TextOption* text_option(const std::string& word) {
  const auto* option = std::find_if(std::begin(kTextOptions), std::end(kTextOptions),
                                    [&word](const TextOption& known) { return word == known.name; });
  return option == std::end(kTextOptions) ? nullptr : option;
}

// Reads `option` and the word it is given.
std::string read_text_option(const TextOption& option, const std::vector<std::string>& args, std::size_t& next,
                             ServerSettings& settings) {
  ++next;
  const auto value = command_line::option_value(args, next);
  if (!value) return tr(option.missing);
  option.set(settings, *value);
  return {};
}

// Reads the word at `next` (and an option's number); an error when it makes no sense.
std::string read_word(const std::vector<std::string>& args, std::size_t& next, ServerSettings& settings) {
  if (const TextOption* option = text_option(args[next])) return read_text_option(*option, args, next, settings);
  if (command_line::is_option(args[next])) return command_line::read_number_option(kOptions, args, next, settings);
  if (!settings.game_dir.empty()) return command_line::unexpected(args[next]);
  settings.game_dir = args[next++];
  return {};
}

// Whether the line names an address for a status port it does not ask for.
bool binds_no_status_port(const StatusSettings& status) {
  return !status.port && status.address != kDefaultStatusAddress;
}

// What makes no sense in a line read whole, if anything does.
std::string missing_from(const ServerSettings& settings) {
  if (settings.game_dir.empty()) return tr("server.cli.no_game");
  if (binds_no_status_port(settings.status)) return tr("server.cli.status_bind_needs_port");
  return {};
}

// Reads the whole line into `settings`; what makes no sense in it, if anything does.
std::string read_line(const std::vector<std::string>& args, ServerSettings& settings) {
  for (std::size_t next = 0; next < args.size();) {
    std::string error = read_word(args, next, settings);
    if (!error.empty()) return error;
  }
  return missing_from(settings);
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
