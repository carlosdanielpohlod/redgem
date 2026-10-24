#include "net/discovery/nearby_games.hpp"

#include <algorithm>
#include <limits>

#include <nlohmann/json.hpp>

#include "core/safe_path.hpp"
#include "net/private_address.hpp"
#include "net/socket/socket.hpp"
#include "net/wire.hpp"
#include "net/wire_codec.hpp"

namespace redgem::net {

namespace {

constexpr const char* kAnswerMark = "redgem";  // every answer says so, under kMark
constexpr const char* kMark = "is";
constexpr const char* kNameKey = "name";
constexpr const char* kPortKey = "port";
constexpr const char* kPlayersKey = "players";
constexpr std::size_t kMaxGames = 32;
constexpr int kMaxGamesPerAddress = 4;  // one machine cannot fill the list by itself
constexpr std::size_t kMaxGameNameBytes = 48;
constexpr int kMaxPlayersShown = 99;
constexpr int kLargestPort = 65535;

bool has_text(const nlohmann::json& answer, const char* key) { return answer.contains(key) && answer[key].is_string(); }

bool has_whole_number(const nlohmann::json& answer, const char* key) {
  return answer.contains(key) && answer[key].is_number_integer();
}

// Whether an answer says it is a game of this engine's, of whatever version.
bool from_redgem(const nlohmann::json& answer) {
  return answer.is_object() && answer.value(kMark, nlohmann::json()) == kAnswerMark;
}

// Anything can arrive on a port: only a well-formed answer counts.
bool is_answer(const nlohmann::json& answer) {
  return has_text(answer, kNameKey) && has_whole_number(answer, kPortKey) && has_whole_number(answer, kPlayersKey);
}

// What a game's name is shown as: as much of it as a list holds (nothing in it drives a terminal:
// it is shown on screens and in logs).
std::string shown_name(const nlohmann::json& answer) {
  return has_text(answer, kNameKey) ? printable(answer[kNameKey].get<std::string>().substr(0, kMaxGameNameBytes))
                                    : std::string();
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

// The game a well-formed answer from `from` describes, on `port`.
NearbyGame game_of(const nlohmann::json& answer, std::uint16_t port, const std::string& from) {
  NearbyGame game;
  game.address = from;
  game.port = port;
  game.name = shown_name(answer);
  game.players = static_cast<int>(std::clamp<std::int64_t>(wide_number(answer[kPlayersKey]), 1, kMaxPlayersShown));
  game.protocol = wire::kProtocolVersion;
  return game;
}

// A game of another version (or none): what it says may be shaped as that version's answers are,
// so only who it is is read, for the searcher to say why it cannot be joined.
NearbyGame game_of_another_protocol(const nlohmann::json& answer, const std::string& from) {
  NearbyGame game;
  game.address = from;
  game.name = shown_name(answer);
  game.protocol = wire::protocol_of(answer);
  return game;
}

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

bool nearby_address(const std::string& address) { return is_private_address(address); }

bool search_to_answer(const std::string& message, const std::string& from) {
  return message == kSearchQuestion && nearby_address(from);
}

std::string search_answer(const std::string& name, std::uint16_t game_port, int players) {
  const nlohmann::json answer = {{kMark, kAnswerMark},
                                 {kNameKey, name.substr(0, kMaxGameNameBytes)},
                                 {kPortKey, game_port},
                                 {kPlayersKey, players},
                                 {wire::kVersion, wire::kProtocolVersion}};
  const std::vector<std::uint8_t> bytes = wire::msgpack(answer);
  return {bytes.begin(), bytes.end()};
}

std::optional<NearbyGame> game_from_answer(const std::string& message, const std::string& from) {
  if (!nearby_address(from)) return std::nullopt;  // a game on this network is not heard of from far away
  const auto answer = wire::unpacked(message);
  if (!answer || !from_redgem(*answer)) return std::nullopt;
  if (!wire::speaks_our_protocol(*answer)) return game_of_another_protocol(*answer, from);
  if (!is_answer(*answer)) return std::nullopt;
  const std::int64_t port = wide_number((*answer)[kPortKey]);
  if (!is_port(port)) return std::nullopt;
  return game_of(*answer, static_cast<std::uint16_t>(port), from);
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
