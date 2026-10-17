#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/discovery/lan_discovery.hpp"
#include "net/wire.hpp"

using namespace redgem::net;
using nlohmann::json;

namespace {

constexpr const char* kNeighbour = "192.168.0.12";
constexpr const char* kOtherNeighbour = "192.168.0.13";
constexpr std::uint16_t kGamePort = 7777;
constexpr std::uint16_t kOtherGamePort = 7780;
constexpr int kPlayers = 3;
constexpr int kMostPlayersShown = 99;
constexpr int kTooManyPlayers = 5000;
constexpr int kLargestPort = 65535;
constexpr std::size_t kLongestName = 48;
constexpr int kGamesOneAddressMayList = 4;
constexpr int kGamesListed = 32;
constexpr int kAddressesTried = 40;
constexpr std::int64_t kPastThirtyTwoBits = std::int64_t{1} << 32;
constexpr std::int64_t kWrapsToGamePort = kPastThirtyTwoBits + kGamePort;
constexpr std::int64_t kWrapsToPlayers = kPastThirtyTwoBits + kPlayers;
constexpr std::uint64_t kBeyondSigned = ~std::uint64_t{0};  // the largest number an answer can carry

// What travels in a datagram: MessagePack.
std::string as_bytes(const json& message) {
  const std::vector<std::uint8_t> bytes = json::to_msgpack(message);
  return {bytes.begin(), bytes.end()};
}

std::string answer(const json& name, const json& port, const json& players,
                   const json& version = wire::kProtocolVersion) {
  return as_bytes({{"is", "redgem"}, {"name", name}, {"port", port}, {"players", players}, {"v", version}});
}

NearbyGame game_at(const std::string& address, std::uint16_t port, const std::string& name = "Maze") {
  NearbyGame game;
  game.address = address;
  game.port = port;
  game.name = name;
  game.players = 1;
  return game;
}

}  // namespace

TEST_CASE("only machines on the local network are nearby") {
  for (const char* local : {"127.0.0.1", "10.0.0.5", "10.255.3.1", "192.168.0.12", "192.168.255.255", "172.16.0.1",
                            "172.31.9.9", "169.254.10.20"}) {
    CAPTURE(local);
    CHECK(nearby_address(local));
  }
  for (const char* elsewhere : {"8.8.8.8", "192.169.0.1", "192.167.1.1", "172.15.0.1", "172.32.0.1", "169.253.0.1",
                                "11.0.0.1", "126.0.0.1", "128.0.0.1"}) {
    CAPTURE(elsewhere);
    CHECK_FALSE(nearby_address(elsewhere));
  }
}

TEST_CASE("what is not an address in numbers is not nearby") {
  for (const char* odd : {"", "localhost", "10", "10.", "192.x.0.1", ".168.0.1", "192,168.0.1"}) {
    CAPTURE(odd);
    CHECK_FALSE(nearby_address(odd));
  }
}

TEST_CASE("an answer to a search says a game's name, port, players and version of the protocol") {
  const auto game = game_from_answer(search_answer("Maze", kGamePort, kPlayers), kNeighbour);
  REQUIRE(game.has_value());
  CHECK(game->address == kNeighbour);
  CHECK(game->port == kGamePort);
  CHECK(game->name == "Maze");
  CHECK(game->players == kPlayers);
  CHECK(game->protocol == wire::kProtocolVersion);
  CHECK(game->seen_ago == 0.0f);
  // A game of another version is still described: the searcher says why it cannot be joined.
  CHECK(game_from_answer(answer("Maze", kGamePort, kPlayers, wire::kProtocolVersion + 1), kNeighbour)->protocol ==
        wire::kProtocolVersion + 1);
}

TEST_CASE("an answer's name is cut and its players kept within what a list shows") {
  const auto game =
      game_from_answer(answer(std::string(kLongestName + 1, 'n'), kGamePort, kTooManyPlayers), kNeighbour);
  REQUIRE(game.has_value());
  CHECK(game->name.size() == kLongestName);
  CHECK(game->players == kMostPlayersShown);
  CHECK(game_from_answer(answer("Maze", kGamePort, 0), kNeighbour)->players == 1);
  CHECK(game_from_answer(answer("Maze", kGamePort, -kPlayers), kNeighbour)->players == 1);
}

TEST_CASE("anything that is not a whole answer is no game") {
  CHECK_FALSE(game_from_answer("", kNeighbour).has_value());
  CHECK_FALSE(game_from_answer("redgem: who is hosting?", kNeighbour).has_value());
  CHECK_FALSE(game_from_answer("[1, 2]", kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(as_bytes(json::array({1, 2})), kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(as_bytes({{"name", "Maze"}, {"port", kGamePort}, {"players", 1}, {"v", 1}}), kNeighbour)
                  .has_value());
  CHECK_FALSE(
      game_from_answer(as_bytes({{"is", "other"}, {"name", "Maze"}, {"port", kGamePort}, {"players", 1}, {"v", 1}}),
                       kNeighbour)
          .has_value());
  // An answer in JSON text, as versions before MessagePack sent it.
  CHECK_FALSE(
      game_from_answer(json::from_msgpack(search_answer("Maze", kGamePort, kPlayers)).dump(), kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(answer(kPlayers, kGamePort, kPlayers), kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(answer("Maze", "7777", kPlayers), kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(answer("Maze", kGamePort, "many"), kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(answer("Maze", 0, kPlayers), kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(answer("Maze", -1, kPlayers), kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(answer("Maze", kLargestPort + 1, kPlayers), kNeighbour).has_value());
  CHECK(game_from_answer(answer("Maze", kLargestPort, kPlayers), kNeighbour).has_value());
}

TEST_CASE("a game that answers again takes the place of what was known of it") {
  std::vector<NearbyGame> games;
  list_game(games, game_at(kNeighbour, kGamePort));
  list_game(games, game_at(kOtherNeighbour, kGamePort));
  NearbyGame grown = game_at(kNeighbour, kGamePort, "Maze II");
  grown.players = kPlayers;
  list_game(games, grown);
  REQUIRE(games.size() == 2);
  CHECK(games.front().name == "Maze II");
  CHECK(games.front().players == kPlayers);
  // Another port on the same machine is another game.
  list_game(games, game_at(kNeighbour, kOtherGamePort));
  CHECK(games.size() == 3);
}

TEST_CASE("a game on this very machine is listed once, by the address the others see") {
  std::vector<NearbyGame> games;
  list_game(games, game_at(kThisMachine, kGamePort));
  list_game(games, game_at(kNeighbour, kGamePort));
  REQUIRE(games.size() == 1);
  CHECK(games.front().address == kNeighbour);
  // Its answer as this machine keeps arriving, and changes nothing.
  list_game(games, game_at(kThisMachine, kGamePort));
  REQUIRE(games.size() == 1);
  CHECK(games.front().address == kNeighbour);
  // A game of another name on that port is not its twin.
  list_game(games, game_at(kThisMachine, kGamePort, "Race"));
  CHECK(games.size() == 2);
}

TEST_CASE("one address lists only a few games, and the list only so many") {
  std::vector<NearbyGame> games;
  for (int i = 0; i <= kGamesOneAddressMayList; ++i) {
    list_game(games, game_at(kNeighbour, static_cast<std::uint16_t>(kGamePort + i), "Game" + std::to_string(i)));
  }
  CHECK(games.size() == kGamesOneAddressMayList);
  games.clear();
  for (int i = 0; i < kAddressesTried; ++i) list_game(games, game_at("10.0.0." + std::to_string(i), kGamePort));
  CHECK(games.size() == kGamesListed);
}

TEST_CASE("an answer from an address that is not nearby describes no game") {
  CHECK(game_from_answer(answer("Maze", kGamePort, kPlayers), kNeighbour).has_value());
  for (const char* far : {"8.8.8.8", "203.0.113.9", "172.32.0.1", "", "somewhere"}) {
    CAPTURE(far);
    CHECK_FALSE(game_from_answer(answer("Maze", kGamePort, kPlayers), far).has_value());
  }
}

TEST_CASE("numbers too large for a port or a count of players do not come out as small ones") {
  // (Narrowed to 32 bits these would read as port 7777 and as 3 players.)
  CHECK_FALSE(game_from_answer(answer("Maze", kWrapsToGamePort, kPlayers), kNeighbour).has_value());
  CHECK_FALSE(game_from_answer(answer("Maze", kBeyondSigned, kPlayers), kNeighbour).has_value());
  const auto crowded = game_from_answer(answer("Maze", kGamePort, kWrapsToPlayers), kNeighbour);
  REQUIRE(crowded.has_value());
  CHECK(crowded->players == kMostPlayersShown);
  CHECK(game_from_answer(answer("Maze", kGamePort, kBeyondSigned), kNeighbour)->players == kMostPlayersShown);
  CHECK(game_from_answer(answer("Maze", kGamePort, -kWrapsToPlayers), kNeighbour)->players == 1);
}

TEST_CASE("a game's name cannot carry what drives a terminal") {
  const auto game = game_from_answer(answer("Maze\x1b[2J\nfake line", kGamePort, kPlayers), kNeighbour);
  REQUIRE(game.has_value());
  CHECK(game->name == "Maze?[2J?fake line");
}

TEST_CASE("a game of another version is described whatever its answer holds, for the searcher to say why") {
  // No version, one that is not a number, and a newer one whose answer has another shape altogether.
  for (const std::string& other :
       {as_bytes({{"is", "redgem"}, {"name", "Maze"}, {"port", kGamePort}, {"players", 1}}),
        answer("Maze", kGamePort, kPlayers, "1"),
        as_bytes(
            {{"is", "redgem"}, {"name", "Maze"}, {"where", {{"port", "7777"}}}, {"v", wire::kProtocolVersion + 1}})}) {
    const auto game = game_from_answer(other, kNeighbour);
    REQUIRE(game.has_value());
    CHECK(game->protocol != wire::kProtocolVersion);
    CHECK(game->name == "Maze");
    CHECK(game->address == kNeighbour);
  }
  CHECK(game_from_answer(as_bytes({{"is", "redgem"}, {"v", wire::kProtocolVersion + 1}}), kNeighbour)->name.empty());
}
