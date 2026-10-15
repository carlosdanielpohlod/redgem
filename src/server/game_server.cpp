// GameServer: starting and stopping, and the turn every room gets.

#include "server/game_server.hpp"

#include <variant>

#include "app/scene_files.hpp"
#include "core/log.hpp"

namespace redgem::server {

bool GameServer::start(std::string& error) {
  auto scene = read_game_scene(settings_.game_dir);
  if (const auto* failed = std::get_if<std::string>(&scene)) {
    error = *failed;
    return false;
  }
  scene_ = std::move(std::get<SceneDesc>(scene));
  return lobby_.open(settings_.port, error);
}

void GameServer::tick(float seconds) {
  const auto in_rooms = [this](const std::string& address) { return players_from(address); };
  for (net::Arrival& arrival : lobby_.poll(seconds, in_rooms)) place(std::move(arrival));
  for (Room& room : rooms_) room.engine->frame(seconds);
  close_rooms_whose_match_is_over();
}

void GameServer::stop() {
  for (Room& room : rooms_) close(room);
  rooms_.clear();
  lobby_.close();
}

std::size_t GameServer::player_count() const {
  std::size_t players = 0;
  for (const Room& room : rooms_) players += room.network().peers().size();
  return players;
}

Engine* GameServer::room(const std::string& code) {
  Room* found = find_room(code);
  return found != nullptr ? found->engine.get() : nullptr;
}

int GameServer::players_from(const std::string& address) const {
  int players = 0;
  for (const Room& room : rooms_) players += room.network().links_from(address);
  return players;
}

void GameServer::close_rooms_whose_match_is_over() {
  for (auto room = rooms_.begin(); room != rooms_.end();) {
    if (!room->match_over()) {
      ++room;
      continue;
    }
    const char* why = room->engine->finished() ? "the match ended" : "everyone left";
    log::info("room " + room->code + " closed: " + why);
    close(*room);
    room = rooms_.erase(room);
  }
}

void GameServer::close(Room& room) {
  // A last try to deliver what the game said; then its players hear that the game is gone.
  room.network().leave();
}

}  // namespace redgem::server
