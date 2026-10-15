#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/engine.hpp"
#include "net/lobby.hpp"
#include "net/wire.hpp"
#include "scene/scene_desc.hpp"
#include "server/room_choice.hpp"

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
// Where a server keeps what its game saves (Save), unless told: a folder beside the game's,
// named after it with this ("games/platformer" -> "games/platformer-data"), holding one file.
inline constexpr const char* kDataFolderSuffix = "-data";
inline constexpr const char* kSaveFileName = "save.json";

// The folder beside the game's that keeps its saves on a server.
std::filesystem::path data_dir_beside(const std::filesystem::path& game_dir);

// What a dedicated server is asked to be.
struct ServerSettings {
  std::filesystem::path game_dir;
  std::uint16_t port = kDefaultServerPort;  // 0 picks a free one (see GameServer::port)
  RoomLimits limits{kDefaultMaxPlayers, kDefaultMaxRooms};
  std::filesystem::path data_dir;  // where Save is kept; empty: nothing is written (kept in memory)
};

// A game played on a server with nobody at it (redgem-server): everyone joins, nobody hosts. One
// port, several rooms; each room is a match of its own, the game running with no window in an
// engine of its own (where Network.server? is true). A room opens for the first player who
// needs one and closes when its match ends: the game quits (Game.quit), or everyone left. Its
// players are told the game is gone, and the next ones get a new room, with a new game.
// What the game saves (Save) is the server's, kept in its data folder for every room at once and
// for the next time it runs. Rooms take their turns one after the other on the server's one
// thread, so their writes never meet: each is written whole, at once, before the next.
class GameServer {
 public:
  explicit GameServer(ServerSettings settings) : settings_(std::move(settings)) {}
  GameServer(const GameServer&) = delete;
  GameServer& operator=(const GameServer&) = delete;

  // Reads the game and opens the port; false with `error` when either cannot be done.
  bool start(std::string& error);
  std::uint16_t port() const { return lobby_.port(); }
  // One turn: newcomers are heard and placed, every room plays `seconds` of its match, and the
  // rooms whose match ended close.
  void tick(float seconds);
  // Ends every match (their players are told the game is gone) and closes the port.
  void stop();

  std::size_t room_count() const { return rooms_.size(); }
  std::size_t player_count() const;
  // The game of the room with that code; nullptr when no room has it.
  Engine* room(const std::string& code);

 private:
  struct Room {
    std::string code;
    std::unique_ptr<Engine> engine;

    net::NetworkSession& network() const { return engine->network(); }
    // Its places taken: by the players, and by the newcomers its game is deciding on.
    std::size_t places_taken() const { return network().peers().size() + network().deciding(); }
    // Whether its match is over: the game quit, or nobody is left in it (nor waiting to come in).
    bool match_over() const { return engine->finished() || places_taken() == 0; }
  };

  void place(net::Arrival arrival);
  // A room opened for a newcomer; nullptr when no code was found for it.
  Room* open_room();
  Room* find_room(const std::string& code);
  std::vector<RoomSeats> seats() const;
  // How many from `address` the rooms hold.
  int players_from(const std::string& address) const;
  void close_rooms_whose_match_is_over();
  static void close(Room& room);

  ServerSettings settings_;
  SceneDesc scene_;  // the game's first scene, read once: every room starts from it
  SaveStore saves_;  // shared by every room
  net::Lobby lobby_;
  std::vector<Room> rooms_;  // in the order they opened
};

}  // namespace redgem::server
