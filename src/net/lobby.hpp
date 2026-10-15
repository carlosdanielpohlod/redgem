#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "net/code_guessers.hpp"
#include "net/connection.hpp"
#include "net/socket.hpp"

namespace redgem::net {

// A machine that connected to a dedicated server and said which room it wants.
struct Arrival {
  Connection connection;
  std::string room;     // the code of the room it asks for; empty for any room with a free place
  nlohmann::json auth;  // what it carries for the room's game to decide on (null: nothing)
};

// How many machines from an address are in the server's rooms already.
using AddressCount = std::function<int(const std::string& address)>;

// Where the machines that connect to a dedicated server (redgem-server) wait until they say which
// room they want: a game joined by the server's address (any room will do), or a room by its code
// (the hello a relay's room is joined with). Nothing here knows the rooms: the server places
// whoever comes out. It faces the internet, so what one address may hold and try is limited, a
// newcomer that keeps quiet is let go, and an address that guesses at room codes is kept out.
class Lobby {
 public:
  // Listens on `port` (0 picks a free one, see port()). False with `error` when it cannot.
  bool open(std::uint16_t port, std::string& error);
  std::uint16_t port() const { return port_; }
  // Takes newcomers in and hears them out: whoever said which room it wants comes back, to be
  // placed. `in_rooms` says how many from an address the rooms hold already.
  std::vector<Arrival> poll(float seconds, const AddressCount& in_rooms);
  // Turns away a machine that asked for a room nobody has: a typo, or a guess.
  void turn_away_guess(Arrival& arrival);
  void close();

 private:
  // A machine that connected and has not been placed yet.
  struct Newcomer {
    Connection connection;
    std::optional<std::string> wants;  // the room it asked for, once it said ("" for any)
    nlohmann::json auth;

    // Said which room it wants, and is still there to be placed.
    bool ready() const { return wants.has_value() && !connection.closed(); }
    // Placed, or gone: it waits here no longer.
    bool done() const { return wants.has_value() || connection.closed(); }
  };

  void accept_newcomers(const AddressCount& in_rooms);
  // Whether one more machine from `address` may wait here.
  bool room_to_wait(const std::string& address, const AddressCount& in_rooms) const;
  int waiting_from(const std::string& address) const;
  void hear(Newcomer& newcomer, float seconds);
  std::vector<Arrival> take_arrivals();

  Socket listener_;
  std::uint16_t port_ = 0;
  std::vector<Newcomer> newcomers_;
  CodeGuessers guessers_;
};

}  // namespace redgem::net
