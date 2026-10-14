#include "net/lan_discovery.hpp"

#include <algorithm>

namespace redgem::net {

namespace {

constexpr float kAskEverySeconds = 1.0f;
constexpr float kForgetAfterSeconds = 3.5f;  // a game that stopped answering is gone
constexpr int kMaxAnswersPerUpdate = 8;      // a flood of questions gets a trickle of answers

}  // namespace

bool LanDiscovery::announce(const std::string& name, std::uint16_t game_port, std::uint16_t discovery_port) {
  std::string ignored;
  announcer_ = Datagrams::open_on(discovery_port, ignored);
  name_ = name;
  game_port_ = game_port;
  players_ = 1;
  return announcer_.valid();
}

void LanDiscovery::stop_announcing() { announcer_.close(); }

bool LanDiscovery::search(std::uint16_t discovery_port) {
  if (searching_on(discovery_port)) return true;
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
  forget_silent_games(seconds);
  since_asked_ += seconds;
  if (since_asked_ >= kAskEverySeconds) {
    since_asked_ = 0.0f;
    ask_around();
  }
  collect_answers();
}

void LanDiscovery::forget_silent_games(float seconds) {
  for (NearbyGame& game : games_) game.seen_ago += seconds;
  games_.erase(std::remove_if(games_.begin(), games_.end(),
                              [](const NearbyGame& game) { return game.seen_ago > kForgetAfterSeconds; }),
               games_.end());
}

void LanDiscovery::ask_around() {
  finder_.send_to(kEveryoneNearby, search_port_, kSearchQuestion);
  finder_.send_to(kThisMachine, search_port_, kSearchQuestion);  // a game on this same machine
}

void LanDiscovery::answer_searches() {
  std::string message;
  std::string from;
  std::uint16_t from_port = 0;
  int answered = 0;
  while (announcer_.receive(message, from, from_port)) {
    if (!search_to_answer(message, from)) continue;
    if (answered >= kMaxAnswersPerUpdate) continue;
    announcer_.send_to(from, from_port, search_answer(name_, game_port_, players_));
    ++answered;
  }
}

void LanDiscovery::collect_answers() {
  std::string message;
  std::string from;
  std::uint16_t from_port = 0;
  while (finder_.receive(message, from, from_port)) {
    if (const auto game = game_from_answer(message, from)) list_game(games_, *game);
  }
}

}  // namespace redgem::net
