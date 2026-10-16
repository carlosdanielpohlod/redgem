#include "net/discovery/nearby_games.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include <nlohmann/json.hpp>

#include "core/safe_path.hpp"
#include "net/socket/socket.hpp"

namespace redgem::net {

namespace {

constexpr const char* kAnswerMark = "redgem";  // every answer says so, under kMark
constexpr const char* kMark = "is";
constexpr const char* kNameKey = "name";
constexpr const char* kPortKey = "port";
constexpr const char* kPlayersKey = "players";
constexpr std::size_t kMaxGames = 32;
constexpr int kMaxGamesPerAddress = 4;  // one machine cannot fill the list by itself
constexpr int kCompact = -1;            // nlohmann's "no indentation"
// The first numbers of the addresses machines have on a local network (and of this machine itself).
constexpr int kThisMachineNet = 127;
constexpr int kPrivateNetA = 10;
constexpr int kPrivateNetB = 172;
constexpr int kPrivateNetBFrom = 16;
constexpr int kPrivateNetBTo = 31;
constexpr int kPrivateNetC = 192;
constexpr int kPrivateNetCSecond = 168;
constexpr int kLinkLocalNet = 169;
constexpr int kLinkLocalSecond = 254;

constexpr std::size_t kMaxNameBytes = 48;
constexpr int kMaxPlayersShown = 99;
constexpr int kLargestPort = 65535;
constexpr char kAddressDot = '.';
constexpr int kDecimal = 10;

// The first two numbers of an address ("192.168.0.12" -> 192, 168); false when it does not start with two.
bool leading_numbers(const std::string& address, long& first, long& second) {
  char* after_first = nullptr;
  first = std::strtol(address.c_str(), &after_first, kDecimal);
  const bool number_then_dot = after_first != address.c_str() && *after_first == kAddressDot;
  if (!number_then_dot) return false;
  const char* second_text = after_first + 1;
  char* after_second = nullptr;
  second = std::strtol(second_text, &after_second, kDecimal);
  return after_second != second_text;
}

bool has_text(const nlohmann::json& answer, const char* key) { return answer.contains(key) && answer[key].is_string(); }

bool has_whole_number(const nlohmann::json& answer, const char* key) {
  return answer.contains(key) && answer[key].is_number_integer();
}

// Anything can arrive on a port: only a well-formed answer counts.
bool is_answer(const nlohmann::json& answer) {
  return answer.is_object() && answer.value(kMark, nlohmann::json()) == kAnswerMark && has_text(answer, kNameKey) &&
         has_whole_number(answer, kPortKey) && has_whole_number(answer, kPlayersKey);
}

// A whole number of an answer in 64 bits, however large it was written (one past what those hold
// counts as the largest there is). Ports and players are checked as this before they are made to
// fit anything smaller: a huge number must not come out as a small one.
std::int64_t wide_number(const nlohmann::json& number) {
  constexpr std::int64_t largest = std::numeric_limits<std::int64_t>::max();
  const bool past_largest =
      number.is_number_unsigned() && number.get<std::uint64_t>() > static_cast<std::uint64_t>(largest);
  return past_largest ? largest : number.get<std::int64_t>();
}

bool is_port(std::int64_t number) { return number >= 1 && number <= kLargestPort; }

std::vector<NearbyGame>::iterator find_listed(std::vector<NearbyGame>& games, const NearbyGame& game) {
  return std::find_if(games.begin(), games.end(), [&game](const NearbyGame& other) {
    return other.address == game.address && other.port == game.port;
  });
}

// A game on this very machine answers twice: as this machine and by its address on the
// network. The second is the one the others see too, so it is the one kept.
std::vector<NearbyGame>::iterator find_twin(std::vector<NearbyGame>& games, const NearbyGame& game) {
  return std::find_if(games.begin(), games.end(), [&game](const NearbyGame& other) {
    return other.port == game.port && other.name == game.name &&
           (other.address == kThisMachine) != (game.address == kThisMachine);
  });
}

bool room_for(const std::vector<NearbyGame>& games, const NearbyGame& game) {
  const auto from_there = std::count_if(games.begin(), games.end(),
                                        [&game](const NearbyGame& other) { return other.address == game.address; });
  return games.size() < kMaxGames && from_there < kMaxGamesPerAddress;
}

}  // namespace

bool nearby_address(const std::string& address) {
  long first = 0;
  long second = 0;
  if (!leading_numbers(address, first, second)) return false;
  return first == kThisMachineNet || first == kPrivateNetA || (first == kPrivateNetC && second == kPrivateNetCSecond) ||
         (first == kPrivateNetB && second >= kPrivateNetBFrom && second <= kPrivateNetBTo) ||
         (first == kLinkLocalNet && second == kLinkLocalSecond);
}

bool search_to_answer(const std::string& message, const std::string& from) {
  return message == kSearchQuestion && nearby_address(from);
}

std::string search_answer(const std::string& name, std::uint16_t game_port, int players) {
  const nlohmann::json answer = {
      {kMark, kAnswerMark}, {kNameKey, name.substr(0, kMaxNameBytes)}, {kPortKey, game_port}, {kPlayersKey, players}};
  return answer.dump(kCompact, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::optional<NearbyGame> game_from_answer(const std::string& message, const std::string& from) {
  if (!nearby_address(from)) return std::nullopt;  // a game on this network is not heard of from far away
  const nlohmann::json answer = nlohmann::json::parse(message, nullptr, false);
  if (!is_answer(answer)) return std::nullopt;
  const std::int64_t port = wide_number(answer[kPortKey]);
  if (!is_port(port)) return std::nullopt;
  NearbyGame game;
  game.address = from;
  game.port = static_cast<std::uint16_t>(port);
  // (The name is shown on screens and in logs: nothing in it drives a terminal.)
  game.name = printable(answer[kNameKey].get<std::string>().substr(0, kMaxNameBytes));
  game.players = static_cast<int>(std::clamp<std::int64_t>(wide_number(answer[kPlayersKey]), 1, kMaxPlayersShown));
  return game;
}

void list_game(std::vector<NearbyGame>& games, const NearbyGame& game) {
  if (const auto listed = find_listed(games, game); listed != games.end()) {
    *listed = game;
    return;
  }
  if (const auto twin = find_twin(games, game); twin != games.end()) {
    if (twin->address == kThisMachine) *twin = game;  // (the kept one answers for itself from here on)
    return;
  }
  if (room_for(games, game)) games.push_back(game);
}

}  // namespace redgem::net
