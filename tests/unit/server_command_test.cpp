#include <doctest/doctest.h>

#include <string>

#include "server/server_command.hpp"
#include "server/server_program.hpp"

using namespace redgem::server;

namespace {

constexpr std::uint16_t kChosenPort = 9000;
constexpr int kChosenPlayers = 4;
constexpr int kChosenRooms = 3;
constexpr int kUsageError = 2;

}  // namespace

TEST_CASE("the server program plays the game it is given, as big as the usual server unless told") {
  const ServerCommand plain = parse_server_command({"games/platformer"});
  REQUIRE(plain.ok);
  CHECK_FALSE(plain.help);
  CHECK(plain.settings.game_dir == "games/platformer");
  CHECK(plain.settings.port == kDefaultServerPort);
  CHECK(plain.settings.limits.max_players == kDefaultMaxPlayers);
  CHECK(plain.settings.limits.max_rooms == kDefaultMaxRooms);
}

TEST_CASE("the server program takes its port and its size, in any order") {
  const ServerCommand chosen =
      parse_server_command({"--rooms", "3", "games/platformer", "--port", "9000", "--max-players", "4"});
  REQUIRE(chosen.ok);
  CHECK(chosen.settings.game_dir == "games/platformer");
  CHECK(chosen.settings.port == kChosenPort);
  CHECK(chosen.settings.limits.max_players == kChosenPlayers);
  CHECK(chosen.settings.limits.max_rooms == kChosenRooms);
  // (Port 0 lets the system pick a free one: the log says which.)
  CHECK(parse_server_command({"game", "--port", "0"}).settings.port == 0);
}

TEST_CASE("the server program's numbers are whole, and within what a server may be") {
  CHECK(parse_server_command({"game", "--max-players", "15"}).ok);
  CHECK(parse_server_command({"game", "--rooms", "64"}).ok);
  for (const char* wrong : {"0", "16", "-1", "4a", "", "123456"}) {
    CAPTURE(wrong);
    CHECK(parse_server_command({"game", "--max-players", wrong}).error == "--max-players needs a number from 1 to 15");
  }
  CHECK(parse_server_command({"game", "--rooms", "65"}).error == "--rooms needs a number from 1 to 64");
  CHECK(parse_server_command({"game", "--port", "65536"}).error == "--port needs a number from 0 to 65535");
  CHECK(parse_server_command({"game", "--port"}).error == "--port needs a number from 0 to 65535");
}

TEST_CASE("the server program keeps the game's saves beside the game, or in the folder it is given") {
  CHECK(parse_server_command({"games/platformer"}).settings.data_dir == "games/platformer-data");
  CHECK(parse_server_command({"/srv/game/"}).settings.data_dir == "/srv/game-data");
  const ServerCommand chosen = parse_server_command({"--data", "/var/lib/platformer", "games/platformer"});
  REQUIRE(chosen.ok);
  CHECK(chosen.settings.data_dir == "/var/lib/platformer");
  CHECK(parse_server_command({"games/platformer", "--data"}).error == "--data needs a folder");
  CHECK(server_usage().find("--data <dir>") != std::string::npos);
}

TEST_CASE("what the server program does not understand is refused by name") {
  CHECK(parse_server_command({}).error == "which game? redgem-server <game_dir>");
  CHECK(parse_server_command({"game", "other"}).error == "unexpected argument other");
  CHECK(parse_server_command({"game", "--bogus"}).error == "unexpected argument --bogus");
}

TEST_CASE("the server program's help wins over the rest of its line and names the program") {
  for (const char* option : {"--help", "-h"}) {
    const ServerCommand help = parse_server_command({"--port", "many", option});
    CHECK(help.ok);
    CHECK(help.help);
  }
  const std::string text = server_usage();
  CHECK(text.rfind("usage:", 0) == 0);
  CHECK(text.find("redgem-server <game_dir>") != std::string::npos);
  CHECK(text.find(std::to_string(kDefaultServerPort)) != std::string::npos);
}

TEST_CASE("the server program fails, with the reason said, on a line it cannot read or a game it cannot find") {
  CHECK(run_server_program({"--port", "many"}) == kUsageError);
  CHECK(run_server_program({"--help"}) == 0);
  CHECK(run_server_program({"/no/such/game", "--port", "0"}) == 1);
}
