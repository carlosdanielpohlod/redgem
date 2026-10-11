#include "net/lan_discovery.hpp"

#include <algorithm>
#include <cstdio>

#include <nlohmann/json.hpp>

namespace redgem::net {

namespace {

constexpr const char* kQuestion = "redgem: who is hosting?";
constexpr const char* kAnswerMark = "redgem";   // every answer says so, under kMark
constexpr const char* kMark = "is";
constexpr const char* kNameKey = "name";
constexpr const char* kPortKey = "port";
constexpr const char* kPlayersKey = "players";
constexpr float kAskEverySeconds = 1.0f;
constexpr float kForgetAfterSeconds = 3.5f;   // a game that stopped answering is gone
constexpr std::size_t kMaxGames = 32;
constexpr int kMaxGamesPerAddress = 4;     // one machine cannot fill the list by itself
constexpr int kMaxAnswersPerUpdate = 8;    // a flood of questions gets a trickle of answers
constexpr int kCompact = -1;               // nlohmann's "no indentation"
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

// Whether an address is one a machine on the local network has. Questions from anywhere else
// are not answered: a game must not be made to send messages to strangers.
bool nearby(const std::string& address) {
  int first = 0;
  int second = 0;
  if (std::sscanf(address.c_str(), "%d.%d.", &first, &second) != 2) return false;
  return first == kThisMachineNet || first == kPrivateNetA || (first == kPrivateNetC && second == kPrivateNetCSecond) ||
         (first == kPrivateNetB && second >= kPrivateNetBFrom && second <= kPrivateNetBTo) ||
         (first == kLinkLocalNet && second == kLinkLocalSecond);
}
constexpr std::size_t kMaxNameBytes = 48;
constexpr int kMaxPlayersShown = 99;
constexpr int kLargestPort = 65535;

}  // namespace

bool LanDiscovery::announce(const std::string& name, std::uint16_t game_port, std::uint16_t discovery_port) {
  std::string ignored;
  announcer_ = Datagrams::open_on(discovery_port, ignored);
  name_ = name.substr(0, kMaxNameBytes);
  game_port_ = game_port;
  players_ = 1;
  return announcer_.valid();
}

void LanDiscovery::stop_announcing() { announcer_.close(); }

bool LanDiscovery::search(std::uint16_t discovery_port) {
  if (finder_.valid() && search_port_ == discovery_port) return true;
  std::string ignored;
  finder_ = Datagrams::open_on(0, ignored);
  search_port_ = discovery_port;
  games_.clear();
  since_asked_ = kAskEverySeconds;  // ask right away
  return finder_.valid();
}

void LanDiscovery::stop_searching() {
  finder_.close();
  games_.clear();
}

void LanDiscovery::update(float seconds) {
  if (announcer_.valid()) answer_searches();
  if (!finder_.valid()) return;
  for (NearbyGame& game : games_) game.seen_ago += seconds;
  games_.erase(std::remove_if(games_.begin(), games_.end(),
                              [](const NearbyGame& game) { return game.seen_ago > kForgetAfterSeconds; }),
               games_.end());
  since_asked_ += seconds;
  if (since_asked_ >= kAskEverySeconds) {
    since_asked_ = 0.0f;
    ask_around();
  }
  collect_answers();
}

void LanDiscovery::ask_around() {
  finder_.send_to(kEveryoneNearby, search_port_, kQuestion);
  finder_.send_to(kThisMachine, search_port_, kQuestion);  // a game on this same machine
}

void LanDiscovery::answer_searches() {
  std::string message;
  std::string from;
  std::uint16_t from_port = 0;
  int answered = 0;
  while (announcer_.receive(message, from, from_port)) {
    if (message != kQuestion || !nearby(from) || answered >= kMaxAnswersPerUpdate) continue;
    const nlohmann::json answer = {{kMark, kAnswerMark}, {kNameKey, name_}, {kPortKey, game_port_}, {kPlayersKey, players_}};
    announcer_.send_to(from, from_port, answer.dump(kCompact, ' ', false, nlohmann::json::error_handler_t::replace));
    ++answered;
  }
}

void LanDiscovery::collect_answers() {
  std::string message;
  std::string from;
  std::uint16_t from_port = 0;
  while (finder_.receive(message, from, from_port)) {
    // Anything can arrive on a port: only a well-formed answer counts.
    const nlohmann::json answer = nlohmann::json::parse(message, nullptr, false);
    if (!answer.is_object() || answer.value(kMark, nlohmann::json()) != kAnswerMark) continue;
    if (!answer.contains(kNameKey) || !answer[kNameKey].is_string()) continue;
    if (!answer.contains(kPortKey) || !answer[kPortKey].is_number_integer()) continue;
    if (!answer.contains(kPlayersKey) || !answer[kPlayersKey].is_number_integer()) continue;
    const int port = answer[kPortKey].get<int>();
    if (port <= 0 || port > kLargestPort) continue;
    NearbyGame game;
    game.address = from;
    game.port = static_cast<std::uint16_t>(port);
    game.name = answer[kNameKey].get<std::string>().substr(0, kMaxNameBytes);
    game.players = std::clamp(answer[kPlayersKey].get<int>(), 1, kMaxPlayersShown);
    const auto known = std::find_if(games_.begin(), games_.end(), [&game](const NearbyGame& other) {
      return other.address == game.address && other.port == game.port;
    });
    // A game on this very machine answers twice: as this machine and by its address on the
    // network. The second is the one the others see too, so it is the one kept.
    const auto twin = std::find_if(games_.begin(), games_.end(), [&game](const NearbyGame& other) {
      return other.port == game.port && other.name == game.name &&
             (other.address == kThisMachine) != (game.address == kThisMachine);
    });
    const auto from_there = std::count_if(games_.begin(), games_.end(), [&game](const NearbyGame& other) {
      return other.address == game.address;
    });
    if (known != games_.end()) {
      *known = game;
    } else if (twin != games_.end()) {
      if (twin->address == kThisMachine) *twin = game;  // (the kept one answers for itself from here on)
    } else if (games_.size() < kMaxGames && from_there < kMaxGamesPerAddress) {
      games_.push_back(game);
    }
  }
}

}  // namespace redgem::net
