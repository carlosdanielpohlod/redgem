#include <doctest/doctest.h>

#include <string>

#include "net/wire.hpp"

using namespace redgem::net;
using nlohmann::json;

namespace {

constexpr int kGuest = 3;
constexpr int kOtherGuest = 4;
constexpr int kPoints = 12;
constexpr std::size_t kTooLongName = kMaxNameBytes + 1;

// Arrays in arrays, `depth` of them.
json nested(int depth) {
  json data = json::array();
  for (int i = 1; i < depth; ++i) data = json::array({data});
  return data;
}

}  // namespace

TEST_CASE("a game message carries its name, who sent it, who it is for and its data") {
  const json message = wire::game_message("score", {{"points", kPoints}}, kGuest, kEveryone);
  CHECK(message[wire::kKind] == wire::kGameMessage);
  CHECK(message[wire::kName] == "score");
  CHECK(message[wire::kFrom] == kGuest);
  CHECK(message[wire::kTo] == kEveryone);
  CHECK(message[wire::kData]["points"] == kPoints);
  CHECK(wire::well_formed(message));
}

TEST_CASE("only an object that says its kind in a text is a message") {
  CHECK(wire::well_formed(json{{wire::kKind, wire::kPing}}));
  CHECK_FALSE(wire::well_formed(json::array()));
  CHECK_FALSE(wire::well_formed(json("hello")));
  CHECK_FALSE(wire::well_formed(json::object()));
  CHECK_FALSE(wire::well_formed(json{{wire::kKind, kGuest}}));
  CHECK_FALSE(wire::well_formed(json::parse("{ not json", nullptr, false)));
}

TEST_CASE("ids in a message are whole numbers no machine could not have") {
  for (const char* key : {wire::kFrom, wire::kTo}) {
    CAPTURE(key);
    CHECK(wire::well_formed(json{{wire::kKind, wire::kGameMessage}, {key, kEveryone}}));
    CHECK(wire::well_formed(json{{wire::kKind, wire::kGameMessage}, {key, kMaxPeers}}));
    CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kGameMessage}, {key, kMaxPeers + 1}}));
    CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kGameMessage}, {key, -1}}));
    CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kGameMessage}, {key, "2"}}));
    CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kGameMessage}, {key, 2.5}}));
  }
}

TEST_CASE("names and room codes are texts of a limited length") {
  for (const char* key : {wire::kName, wire::kRoom}) {
    CAPTURE(key);
    CHECK(wire::well_formed(json{{wire::kKind, wire::kHello}, {key, std::string(kMaxNameBytes, 'a')}}));
    CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kHello}, {key, std::string(kTooLongName, 'a')}}));
    CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kHello}, {key, kGuest}}));
  }
}

TEST_CASE("the peers of a welcome are a list of whole numbers") {
  CHECK(wire::well_formed(json{{wire::kKind, wire::kWelcome}, {wire::kPeers, json::array()}}));
  CHECK(wire::well_formed(json{{wire::kKind, wire::kWelcome}, {wire::kPeers, {kHostId, kGuest}}}));
  CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kWelcome}, {wire::kPeers, kGuest}}));
  CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kWelcome}, {wire::kPeers, {kHostId, "3"}}}));
}

TEST_CASE("data nests only so deep, under keys only so long") {
  CHECK(acceptable_data(json()));
  CHECK(acceptable_data(json("text")));
  CHECK(acceptable_data(nested(kMaxDataDepth)));
  CHECK_FALSE(acceptable_data(nested(kMaxDataDepth + 1)));
  CHECK(acceptable_data(json{{std::string(kMaxNameBytes, 'k'), true}}));
  CHECK_FALSE(acceptable_data(json{{std::string(kTooLongName, 'k'), true}}));
  CHECK_FALSE(acceptable_data(json::array({json{{std::string(kTooLongName, 'k'), true}}})));
  CHECK_FALSE(wire::well_formed(json{{wire::kKind, wire::kGameMessage}, {wire::kData, nested(kMaxDataDepth + 1)}}));
}

TEST_CASE("a newcomer gets the lowest id nobody has, and none in a full game") {
  CHECK(wire::free_peer_id({}) == kFirstGuestId);
  CHECK(wire::free_peer_id({kFirstGuestId, kOtherGuest}) == kGuest);
  std::vector<int> everyone;
  for (int id = kFirstGuestId; id <= kMaxPeers; ++id) everyone.push_back(id);
  CHECK(wire::free_peer_id(everyone) == 0);
}

TEST_CASE("a welcome cannot make a guest the host, or the opener of a room anything else") {
  const bool through_relay = true;
  const bool opening_room = true;
  // Joining a game, directly or in a room: any guest's id, never the host's or nobody's.
  for (const bool relayed : {false, through_relay}) {
    CAPTURE(relayed);
    CHECK(wire::own_id_believable(kFirstGuestId, relayed, !opening_room));
    CHECK(wire::own_id_believable(kGuest, relayed, !opening_room));
    CHECK_FALSE(wire::own_id_believable(kHostId, relayed, !opening_room));
    CHECK_FALSE(wire::own_id_believable(0, relayed, !opening_room));
    CHECK_FALSE(wire::own_id_believable(-1, relayed, !opening_room));
  }
  // Opening a room: the host's id and no other.
  CHECK(wire::own_id_believable(kHostId, through_relay, opening_room));
  CHECK_FALSE(wire::own_id_believable(kGuest, through_relay, opening_room));
  CHECK_FALSE(wire::own_id_believable(0, through_relay, opening_room));
}

TEST_CASE("a packed message is its JSON behind its length, with broken text replaced") {
  const std::vector<std::uint8_t> bytes = wire::packed(json{{wire::kKind, wire::kPing}});
  FrameBuffer buffer;
  buffer.append(bytes.data(), bytes.size());
  const auto text = buffer.next();
  REQUIRE(text.has_value());
  CHECK(json::parse(*text)[wire::kKind] == wire::kPing);
  const std::string not_utf8 = "caf\xE9";
  CHECK_NOTHROW(wire::packed(wire::game_message(not_utf8, json(), kGuest, kEveryone)));
}
