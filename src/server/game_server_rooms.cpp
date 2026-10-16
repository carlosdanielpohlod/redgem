// GameServer: placing the machines that join, and opening rooms for them.

#include <algorithm>
#include <memory>

#include "core/log.hpp"
#include "net/rooms/room_codes.hpp"
#include "server/game_server.hpp"

namespace redgem::server {

void GameServer::place(net::Arrival arrival) {
  const Placement placement = choose_room(seats(), arrival.room, settings_.limits);
  if (placement.kind == Placement::Kind::UnknownRoom) {
    lobby_.turn_away_guess(arrival);
    return;
  }
  const bool new_room = placement.kind == Placement::Kind::NewRoom;
  Room* room = new_room ? open_room() : find_room(placement.code);
  if (!handed_to(room, arrival)) {
    arrival.connection.close();
    return;
  }
  // A new room runs once it holds its first machine: empty, it would close at its first turn.
  if (new_room) run(*room);
}

bool GameServer::handed_to(Room* room, net::Arrival& arrival) {
  // (No room: no code was left for a new one.)
  return room != nullptr && room->slot->hand(arrival);
}

GameServer::Room* GameServer::open_room() {
  const std::string code =
      net::unused_room_code([this](const std::string& taken) { return find_room(taken) != nullptr; });
  if (code.empty()) return nullptr;
  rooms_.push_back({std::make_shared<RoomSlot>(code), &thread_for_new_room()});
  return &rooms_.back();
}

void GameServer::run(Room& room) {
  room.thread->add(room.slot);
  log::info("room " + room.slot->code() + " opened (" + std::to_string(rooms_.size()) + " of " +
            std::to_string(settings_.limits.max_rooms) + ")");
}

RoomThread& GameServer::thread_for_new_room() {
  RoomThread* least_busy = least_busy_thread();
  if (!needs_new_thread(least_busy)) return *least_busy;
  threads_.push_back(std::make_unique<RoomThread>(RoomStart{settings_.game_dir, &scene_, saves_.get()}));
  return *threads_.back();
}

RoomThread* GameServer::least_busy_thread() const {
  const auto least_busy =
      std::min_element(threads_.begin(), threads_.end(),
                       [this](const auto& one, const auto& other) { return rooms_on(*one) < rooms_on(*other); });
  return least_busy != threads_.end() ? least_busy->get() : nullptr;
}

int GameServer::rooms_on(const RoomThread& thread) const {
  return static_cast<int>(
      std::count_if(rooms_.begin(), rooms_.end(), [&thread](const Room& room) { return room.thread == &thread; }));
}

bool GameServer::needs_new_thread(const RoomThread* least_busy) const {
  const bool all_busy = least_busy == nullptr || rooms_on(*least_busy) > 0;
  return all_busy && static_cast<int>(threads_.size()) < settings_.threads;
}

GameServer::Room* GameServer::find_room(const std::string& code) {
  const auto found =
      std::find_if(rooms_.begin(), rooms_.end(), [&code](const Room& room) { return room.slot->code() == code; });
  return found != rooms_.end() ? &*found : nullptr;
}

std::vector<RoomSeats> GameServer::seats() const {
  std::vector<RoomSeats> seats;
  seats.reserve(rooms_.size());
  for (const Room& room : rooms_) {
    // (A room that closed takes nobody more: it is let go at the end of the lobby's turn.)
    if (!room.slot->closed()) seats.push_back({room.slot->code(), static_cast<int>(room.slot->places_taken())});
  }
  return seats;
}

}  // namespace redgem::server
