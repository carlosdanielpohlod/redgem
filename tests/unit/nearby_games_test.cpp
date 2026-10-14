#include <doctest/doctest.h>

#include <string>

#include "net/nearby_games.hpp"

using namespace redgem::net;

namespace {

constexpr const char* kNeighbour = "192.168.0.12";
constexpr const char* kStranger = "8.8.8.8";
constexpr std::uint16_t kGamePort = 7777;
constexpr int kPlayers = 3;
constexpr std::size_t kLongName = 200;  // letters: more than an answer holds
constexpr std::size_t kNameInAnswer = 48;

}  // namespace

TEST_CASE("a host answers the question a neighbour asks, and nothing else") {
  CHECK(search_to_answer(kSearchQuestion, kNeighbour));
  CHECK_FALSE(search_to_answer("who is there?", kNeighbour));
  CHECK_FALSE(search_to_answer(kSearchQuestion, kStranger));
}

TEST_CASE("what a host answers is read back as its game") {
  const auto game = game_from_answer(search_answer("Kart Night", kGamePort, kPlayers), kNeighbour);
  REQUIRE(game.has_value());
  CHECK(game->name == "Kart Night");
  CHECK(game->port == kGamePort);
  CHECK(game->players == kPlayers);
  CHECK(game->address == kNeighbour);
}

TEST_CASE("an answer carries only so much of a long name") {
  const auto game = game_from_answer(search_answer(std::string(kLongName, 'x'), kGamePort, 1), kNeighbour);
  REQUIRE(game.has_value());
  CHECK(game->name == std::string(kNameInAnswer, 'x'));
}

TEST_CASE("a name that is not valid text still gets an answer that can be read") {
  const auto game = game_from_answer(search_answer("caf\xC3", kGamePort, 1), kNeighbour);
  REQUIRE(game.has_value());
  CHECK(game->port == kGamePort);
}
