#include "server/server_command.hpp"

#include <algorithm>
#include <iterator>
#include <optional>

#include "core/command_line.hpp"

namespace redgem::server {

namespace {

constexpr int kLargestPort = 65535;
constexpr std::size_t kMostDigits = 5;  // of any number the line takes
constexpr const char* kDigits = "0123456789";
constexpr const char* kNoGame = "which game? redgem-server <game_dir>";

// An option of the line that is given a whole number, from `least` to `most`.
struct NumberOption {
  const char* name;
  int least;
  int most;
  void (*set)(ServerSettings&, int);
};

constexpr NumberOption kOptions[] = {
    {"--port", 0, kLargestPort,
     [](ServerSettings& settings, int port) { settings.port = static_cast<std::uint16_t>(port); }},
    {"--max-players", 1, kMostPlayersPerRoom,
     [](ServerSettings& settings, int players) { settings.limits.max_players = players; }},
    {"--rooms", 1, kMostRooms, [](ServerSettings& settings, int rooms) { settings.limits.max_rooms = rooms; }},
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
         number(kDefaultMaxRooms) + ", at most " + number(kMostRooms) + ")\n";
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

// Reads the option at `next` and its number; an error when it is no option, or not given a number it takes.
std::string read_option(const std::vector<std::string>& args, std::size_t& next, ServerSettings& settings) {
  const NumberOption* option = find_option(args[next++]);
  if (option == nullptr) return "unexpected argument " + args[next - 1];
  const auto number = number_between(command_line::option_value(args, next).value_or(""), option->least, option->most);
  if (!number) {
    return std::string(option->name) + " needs a number from " + std::to_string(option->least) + " to " +
           std::to_string(option->most);
  }
  option->set(settings, *number);
  return {};
}

// Reads the word at `next` (and an option's number); an error when it makes no sense.
std::string read_word(const std::vector<std::string>& args, std::size_t& next, ServerSettings& settings) {
  if (command_line::is_option(args[next])) return read_option(args, next, settings);
  if (!settings.game_dir.empty()) return "unexpected argument " + args[next];
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
  return command;
}

}  // namespace redgem::server
