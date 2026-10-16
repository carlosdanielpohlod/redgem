#pragma once

#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "app/engine.hpp"
#include "server/room_slot.hpp"

namespace redgem::server {

// What every room of a server starts from, shared by its threads, which only read it: the game,
// its first scene (read once, copied into each room's game), and the saves (SaveDatabase, which
// takes one call at a time from any thread).
struct RoomStart {
  std::filesystem::path game_dir;
  const SceneDesc* scene = nullptr;
  SaveStore* save = nullptr;
};

// One of a dedicated server's threads: it runs the rooms it is given, each a frame every turn,
// on the real clock, until it is stopped. A room's game is made here, its levels loaded here,
// so a level that takes long to load holds up only the rooms of this thread. A room's game is
// its thread's alone; the lobby reaches the room through its RoomSlot.
class RoomThread {
 public:
  explicit RoomThread(RoomStart start);
  ~RoomThread();
  RoomThread(const RoomThread&) = delete;
  RoomThread& operator=(const RoomThread&) = delete;

  // Gives the thread a room to run, from its next turn (from the lobby's thread).
  void add(std::shared_ptr<RoomSlot> slot);
  // Ends the matches of its rooms (their players are told the game is gone) and waits for the
  // thread to end.
  void stop();

 private:
  struct Room {
    std::shared_ptr<RoomSlot> slot;
    std::unique_ptr<Engine> game;  // made at the room's first turn
  };

  void run();
  // Waits `seconds` for the next turn; false when the thread is to stop instead.
  bool wait_for_turn(double seconds);
  void take_new_rooms();
  // One room's frame; a room whose game breaks is closed, the others go on.
  void play_or_close(Room& room, float seconds);
  void play(Room& room, float seconds);
  std::unique_ptr<Engine> open_game(const std::string& code) const;
  // Closes the room and ends its match, saying why in the log.
  static void end(Room& room, const std::string& why);
  void drop_closed_rooms();

  const RoomStart start_;
  std::mutex lock_;  // guards the two below
  std::condition_variable wake_;
  bool stopping_ = false;
  std::vector<std::shared_ptr<RoomSlot>> new_rooms_;
  std::vector<Room> rooms_;  // the thread's own
  std::thread thread_;       // started last, once all of the above is in place
};

}  // namespace redgem::server
