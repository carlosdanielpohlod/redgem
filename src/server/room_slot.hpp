#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "app/engine.hpp"
#include "net/rooms/lobby.hpp"

namespace redgem::server {

// What a dedicated server's lobby and one of its rooms share, the lobby on the server's main
// thread and the room on a thread of its own (see RoomThread). The room's game (its Engine) is
// the room thread's alone: the lobby hands it machines and asks how full it is through this, and
// only through this, under its lock. The room's code never changes.
class RoomSlot {
 public:
  explicit RoomSlot(std::string code) : code_(std::move(code)) {}
  RoomSlot(const RoomSlot&) = delete;
  RoomSlot& operator=(const RoomSlot&) = delete;

  const std::string& code() const { return code_; }

  // --- The lobby's side.
  // Hands the room a machine that asked for it (taken from `arrival`); false when the room closed.
  bool hand(net::Arrival& arrival);
  // Its places taken: by its players, the newcomers its game is deciding on, and those handed to it.
  std::size_t places_taken() const;
  std::size_t players() const;
  // How many machines connected from `address` it holds.
  int links_from(const std::string& address) const;
  bool closed() const;
  // Runs `visitor` with the room's game on the room's thread, between two of its frames, and
  // waits for it. False when the room closed first.
  bool visit(const std::function<void(Engine&)>& visitor);

  // --- The room's side.
  // Lets the machines handed to it into its game, and notes how full it is.
  void admit_arrivals(net::NetworkSession& network);
  void run_visits(Engine& game);
  // Notes how full the game is now, for the lobby.
  void take_census(const net::NetworkSession& network);
  // Closes the room when its match is over: its game quit, or nobody is in it nor on the way.
  // True when it closed.
  bool close_when_over(bool game_quit);
  // Closes it whatever happens (the server stops, or its game broke).
  void close();

 private:
  // A visitor waiting for its turn, and whether it had it (shared by the thread that waits and
  // the room's).
  struct Visit {
    std::function<void(Engine&)> visitor;
    bool done = false;
    bool ran = false;
  };
  using Visits = std::vector<std::shared_ptr<Visit>>;

  // (The ..._locked ones are called with the lock held.)
  std::size_t places_taken_locked() const;
  bool match_over_locked(bool game_quit) const;
  void census_locked(const net::NetworkSession& network);
  void close_locked();
  // Tells the visitors waiting on `visits` they had their turn (or not: `ran`).
  void finish(const Visits& visits, bool ran);

  const std::string code_;
  mutable std::mutex lock_;  // guards everything below
  std::condition_variable visited_;
  std::vector<net::Arrival> arrivals_;  // handed, not let in yet
  Visits visits_;                       // waiting for the room's thread
  std::size_t players_ = 0;             // as of the last census
  std::size_t deciding_ = 0;
  std::vector<std::string> addresses_;
  bool closed_ = false;
};

}  // namespace redgem::server
