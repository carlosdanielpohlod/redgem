#include <doctest/doctest.h>

#include "server/room_choice.hpp"

using namespace redgem::server;

namespace {

constexpr RoomLimits kTwoRoomsOfTwo{2, 2};

}  // namespace

TEST_CASE("quick play opens a room for the first player of a server") {
  const Placement placement = choose_room({}, "", kTwoRoomsOfTwo);
  CHECK(placement.kind == Placement::Kind::NewRoom);
}

TEST_CASE("quick play fills the oldest room with a free place first") {
  const Placement placement = choose_room({{"AAAAAA", 2}, {"BBBBBB", 1}, {"CCCCCC", 0}}, "", {2, 3});
  CHECK(placement.kind == Placement::Kind::Seat);
  CHECK(placement.code == "BBBBBB");
}

TEST_CASE("a full room sends the next player to a new room, while the server may open one") {
  CHECK(choose_room({{"AAAAAA", 2}}, "", kTwoRoomsOfTwo).kind == Placement::Kind::NewRoom);
  CHECK(choose_room({{"AAAAAA", 2}, {"BBBBBB", 2}}, "", kTwoRoomsOfTwo).kind == Placement::Kind::Full);
}

TEST_CASE("a player who asks for a room by its code goes there while it has a place") {
  const std::vector<RoomSeats> rooms{{"AAAAAA", 1}, {"BBBBBB", 2}};
  const Placement placement = choose_room(rooms, "AAAAAA", kTwoRoomsOfTwo);
  CHECK(placement.kind == Placement::Kind::Seat);
  CHECK(placement.code == "AAAAAA");
  // A full room is full for its friends too: they are not put anywhere else instead.
  CHECK(choose_room(rooms, "BBBBBB", kTwoRoomsOfTwo).kind == Placement::Kind::Full);
}

TEST_CASE("a code no room has is no room, even with places free elsewhere") {
  CHECK(choose_room({{"AAAAAA", 0}}, "ZZZZZZ", kTwoRoomsOfTwo).kind == Placement::Kind::UnknownRoom);
  CHECK(choose_room({}, "ZZZZZZ", kTwoRoomsOfTwo).kind == Placement::Kind::UnknownRoom);
}
