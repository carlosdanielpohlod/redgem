#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/engine.hpp"
#include "net/rooms/lobby.hpp"
#include "net/wire.hpp"
#include "scene/scene_desc.hpp"
#include "server/room_choice.hpp"
#include "server/room_slot.hpp"
#include "server/room_thread.hpp"
#include "server/save_database.hpp"

namespace redgem::server {

// Where games join a dedicated server unless told otherwise: the port a game is joined at by
// its address (Network::DEFAULT_PORT).
inline constexpr std::uint16_t kDefaultServerPort = 7777;
// How big a server is unless told otherwise, and at most: the players of one room (every place of
// a game but the host's, which is the server's), and the rooms open at once (each a whole game
// running: memory and time on the server).
inline constexpr int kDefaultMaxPlayers = 8;
inline constexpr int kMostPlayersPerRoom = net::kMaxPeers - 1;
inline constexpr int kDefaultMaxRooms = 8;
inline constexpr int kMostRooms = 64;
// The threads its rooms run on, at most: a room never needs more than one.
inline constexpr int kMostThreads = kMostRooms;
// Where a server keeps what its game saves (Save), unless told: a folder beside the game's,
// named after it with this ("games/platformer" -> "games/platformer-data"), holding a database
// (see SaveDatabase). A server of 0.2.0 kept a JSON file there instead, which is moved into the
// database when the server starts with one there (see save_migration.hpp).
inline constexpr const char* kDataFolderSuffix = "-data";
inline constexpr const char* kSaveDatabaseName = "save.sqlite3";
inline constexpr const char* kSaveFileName = "save.json";

// The folder beside the game's that keeps its saves on a server.
std::filesystem::path data_dir_beside(const std::filesystem::path& game_dir);
// How many threads this machine runs at once (its processors' cores): a server's rooms run on as
// many unless told otherwise.
int processor_count();

// What a dedicated server is asked to be.
struct ServerSettings {
  std::filesystem::path game_dir;
  std::uint16_t port = kDefaultServerPort;  // 0 picks a free one (see GameServer::port)
  RoomLimits limits{kDefaultMaxPlayers, kDefaultMaxRooms};
  int connections_per_address = net::kDefaultConnectionsPerAddress;  // machines one address may connect
  std::filesystem::path data_dir;   // where Save is kept; empty: nothing is written (kept in memory for this run)
  int threads = processor_count();  // the threads the rooms run on, at most
};

// A game played on a server with nobody at it (redgem-server): everyone joins, nobody hosts. One
// port, several rooms; each room is a match of its own, the game running with no window in an
// engine of its own (where Network.server? is true). A room opens for the first player who
// needs one and closes when its match ends: the game quits (Game.quit), or everyone left. Its
// players are told the game is gone, and the next ones get a new room, with a new game.
// The lobby (the port, and who goes to which room) runs on the thread that calls tick; each room
// runs on a thread of the server's (RoomThread), one room per thread while there are no more
// rooms than threads, so that rooms neither wait for each other nor share a core. What the game
// saves (Save) is the server's, kept in its data folder for every room at once and for the next
// time it runs (see SaveDatabase).
class GameServer {
 public:
  explicit GameServer(ServerSettings settings) : settings_(std::move(settings)) {}
  ~GameServer();
  GameServer(const GameServer&) = delete;
  GameServer& operator=(const GameServer&) = delete;

  // Reads the game, opens its saves and the port; false with `error` when any cannot be done.
  bool start(std::string& error);
  std::uint16_t port() const { return lobby_.port(); }
  // The lobby's turn: newcomers are heard and handed to their rooms, and the rooms whose match
  // ended are let go. (The rooms play on their own threads meanwhile, on the real clock.)
  void tick(float seconds);
  // Ends every match (their players are told the game is gone), waits for the rooms' threads to
  // end, and closes the port.
  void stop();

  std::size_t room_count() const { return rooms_.size(); }
  std::size_t player_count() const;
  std::size_t thread_count() const { return threads_.size(); }
  // Runs `visitor` with the game of the room with that code, on the room's thread between two of
  // its frames, and waits for it. False when no room has that code, or it closed first.
  bool visit(const std::string& code, const std::function<void(Engine&)>& visitor);

 private:
  struct Room {
    std::shared_ptr<RoomSlot> slot;
    RoomThread* thread;
  };

  bool open_saves(std::string& error);
  void place(net::Arrival arrival);
  // Whether `room` took the machine: false when there is no room, or it closed meanwhile.
  static bool handed_to(Room* room, net::Arrival& arrival);
  // A room for a newcomer, not running yet (see run); nullptr when no code was found for it.
  Room* open_room();
  void run(Room& room);
  // The thread a new room goes to: one with no room, a new one while there may be more, or else
  // the one with the fewest rooms.
  RoomThread& thread_for_new_room();
  RoomThread* least_busy_thread() const;
  int rooms_on(const RoomThread& thread) const;
  bool needs_new_thread(const RoomThread* least_busy) const;
  Room* find_room(const std::string& code);
  std::vector<RoomSeats> seats() const;
  // How many from `address` the rooms hold.
  int players_from(const std::string& address) const;
  void forget_closed_rooms();

  ServerSettings settings_;
  SceneDesc scene_;                      // the game's first scene, read once: every room starts from it
  std::unique_ptr<SaveDatabase> saves_;  // shared by every room
  net::Lobby lobby_;
  std::vector<Room> rooms_;  // in the order they opened
  std::vector<std::unique_ptr<RoomThread>> threads_;
};

}  // namespace redgem::server
