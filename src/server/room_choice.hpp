#pragma once

#include <string>
#include <vector>

// Where a machine that joins a dedicated server plays: which of the server's rooms it goes to.
namespace redgem::server {

// One of the server's rooms, as a newcomer's place is chosen: its code and how many play in it.
struct RoomSeats {
  std::string code;
  int players = 0;
};

// How big a server is: the players one room holds, and the rooms it may have at once.
struct RoomLimits {
  int max_players = 0;
  int max_rooms = 0;
};

// Where a newcomer goes.
struct Placement {
  enum class Kind {
    Seat,         // into the room with `code`
    NewRoom,      // into a room opened for it
    Full,         // nowhere: the room it asked for, or the whole server, has no place left
    UnknownRoom,  // nowhere: no room has the code it asked for
  };
  Kind kind = Kind::Full;
  std::string code;
};

// The place of a newcomer that asked for the room `wanted`, or for any room when it is empty
// ("quick play": the oldest room with a free place, so that rooms fill one by one). A new room
// opens when every room is full and the server may have one more. `rooms` are in the order they
// opened.
Placement choose_room(const std::vector<RoomSeats>& rooms, const std::string& wanted, const RoomLimits& limits);

}  // namespace redgem::server
