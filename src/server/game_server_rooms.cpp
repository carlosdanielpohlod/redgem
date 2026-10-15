// GameServer: placing the machines that join, and opening rooms for them.

#include <algorithm>

#include "core/log.hpp"
#include "net/room_codes.hpp"
#include "server/game_server.hpp"

namespace redgem::server {

void GameServer::place(net::Arrival arrival) {
  const Placement placement = choose_room(seats(), arrival.room, settings_.limits);
  if (placement.kind == Placement::Kind::UnknownRoom) {
    lobby_.turn_away_guess(arrival);
    return;
  }
  Room* room = placement.kind == Placement::Kind::NewRoom ? open_room() : find_room(placement.code);
  if (room == nullptr) {
    arrival.connection.close();  // no place for it
    return;
  }
  room->network().admit_arrival(std::move(arrival.connection), arrival.auth);
}

GameServer::Room* GameServer::open_room() {
  EngineOptions options;
  options.game_dir = settings_.game_dir;
  options.save = &saves_;
  options.server_room = net::unused_room_code([this](const std::string& code) { return find_room(code) != nullptr; });
  if (options.server_room.empty()) return nullptr;
  rooms_.push_back({options.server_room, Engine::create(options, scene_)});
  log::info("room " + options.server_room + " opened (" + std::to_string(rooms_.size()) + " of " +
            std::to_string(settings_.limits.max_rooms) + ")");
  return &rooms_.back();
}

GameServer::Room* GameServer::find_room(const std::string& code) {
  const auto found =
      std::find_if(rooms_.begin(), rooms_.end(), [&code](const Room& room) { return room.code == code; });
  return found != rooms_.end() ? &*found : nullptr;
}

std::vector<RoomSeats> GameServer::seats() const {
  std::vector<RoomSeats> seats;
  seats.reserve(rooms_.size());
  for (const Room& room : rooms_) seats.push_back({room.code, static_cast<int>(room.places_taken())});
  return seats;
}

}  // namespace redgem::server
