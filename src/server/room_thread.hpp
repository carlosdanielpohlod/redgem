#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "app/engine.hpp"
#include "core/clock.hpp"
#include "server/room_slot.hpp"

namespace redgem::server {

// What every room of a server starts from, shared by its threads, which only read it: the game,
// its first scene (read once, copied into each room's game), the saves (SaveDatabase, which
// takes one call at a time from any thread), and the clock the rooms' turns keep to.
struct RoomStart {
  std::filesystem::path game_dir;
  const SceneDesc* scene = nullptr;
  SaveStore* save = nullptr;
  Clock* clock = &steady_clock();
};

// One of a dedicated server's threads: it runs the rooms it is given, each a frame every turn,
// on the clock it is given (the real one, but in tests), until it is stopped. A room's game is made here, its levels
// loaded here, so a level that takes long to load holds up only the rooms of this thread. A room's game is its thread's
// alone; the lobby reaches the room through its RoomSlot.
class RoomThread {
 public:
  explicit RoomThread(RoomStart start);
  ~RoomThread();
  RoomThread(const RoomThread&) = delete;
  RoomThread& operator=(const RoomThread&) = delete;

  // Gives the thread a room to run, from its next turn (from the lobby's thread).
  void add(std::shared_ptr<RoomSlot> slot);
  // Ends the matches of its rooms (their players are told the game is gone) and waits for the
  // thread to end (a turn's wait at most).
  void stop();

 private:
  struct Room {
    std::shared_ptr<RoomSlot> slot;
    std::unique_ptr<Engine> game;  // made at the room's first turn
  };

  void run();
  // Sleeps `seconds` (a turn at most) for the next turn; false when the thread is to stop instead.
  bool wait_for_turn(double seconds);
  void take_new_rooms();
  // A frame of every room, and how long that took noted in each.
  void play_turn(float seconds);
  // One room's frame; a room whose game breaks is closed, the others go on.
  void play_or_close(Room& room, float seconds);
  void play(Room& room, float seconds);
  // Makes the room's game, the time that takes kept apart (loading_seconds_).
  void open(Room& room);
  std::unique_ptr<Engine> open_game(const std::string& code) const;
  // Closes the room and ends its match, saying why in the log.
  static void end(Room& room, const std::string& why);
  void drop_closed_rooms();

  const RoomStart start_;
  std::mutex lock_;  // guards the two below
  bool stopping_ = false;
  std::vector<std::shared_ptr<RoomSlot>> new_rooms_;
  std::vector<Room> rooms_;       // the thread's own
  double loading_seconds_ = 0.0;  // spent this turn making rooms' games
  std::thread thread_;            // started last, once all of the above is in place
};

}  // namespace redgem::server
