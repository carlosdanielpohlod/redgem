#include "server/room_choice.hpp"

#include <algorithm>

namespace redgem::server {

namespace {

Placement placed(Placement::Kind kind, const std::string& code = {}) { return {kind, code}; }

// Into a room that has a free place.
Placement seat_in(const RoomSeats& room, const RoomLimits& limits) {
  return room.players < limits.max_players ? placed(Placement::Kind::Seat, room.code) : placed(Placement::Kind::Full);
}

Placement any_room(const std::vector<RoomSeats>& rooms, const RoomLimits& limits) {
  const auto free = std::find_if(rooms.begin(), rooms.end(),
                                 [&limits](const RoomSeats& room) { return room.players < limits.max_players; });
  if (free != rooms.end()) return placed(Placement::Kind::Seat, free->code);
  const bool may_open = static_cast<int>(rooms.size()) < limits.max_rooms;
  return placed(may_open ? Placement::Kind::NewRoom : Placement::Kind::Full);
}

}  // namespace

Placement choose_room(const std::vector<RoomSeats>& rooms, const std::string& wanted, const RoomLimits& limits) {
  if (wanted.empty()) return any_room(rooms, limits);
  const auto room =
      std::find_if(rooms.begin(), rooms.end(), [&wanted](const RoomSeats& seats) { return seats.code == wanted; });
  return room != rooms.end() ? seat_in(*room, limits) : placed(Placement::Kind::UnknownRoom);
}

}  // namespace redgem::server
