#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>

#include "net/wire.hpp"

using namespace redgem::net;
using nlohmann::json;

namespace {

constexpr int kGuest = 3;
constexpr int kOtherGuest = 4;
constexpr int kPoints = 12;
constexpr std::size_t kTooLongName = kMaxNameBytes + 1;
constexpr std::int64_t kFarTooLargeId = 5'000'000'000;  // more than 32 bits hold
constexpr double kNotWhole = 2.5;

// Arrays in arrays, `depth` of them.
json nested(int depth) {
  json data = json::array();
  for (int i = 1; i < depth; ++i) data = json::array({data});
  return data;
}

}  // namespace

TEST_CASE("a game message carries its name, who sent it, who it is for and its data") {
  const json message = wire::game_message("score", {{"points", kPoints}}, kGuest, kOtherGuest);
  CHECK(message[wire::kKind] == wire::kGameMessage);
  CHECK(message[wire::kName] == "score");
  CHECK(message[wire::kFrom] == kGuest);
  CHECK(message[wire::kTo] == kOtherGuest);
  CHECK(message[wire::kData]["points"] == kPoints);
  CHECK(wire::well_formed(message));
  // One for everyone does not say so: that is what a message for nobody in particular is.
  CHECK_FALSE(wire::game_message("score", nullptr, kGuest, kEveryone).contains(wire::kTo));
}

TEST_CASE("what is passed on is from whom the host knows it is, to whom it says, and nothing else") {
  json message = wire::game_message("score", kPoints, kHostId, kOtherGuest);
  message["extra"] = "ignored";
  const json passed = wire::passed_on(message, kGuest);
  CHECK(passed[wire::kFrom] == kGuest);
  CHECK(passed[wire::kTo] == kOtherGuest);
  CHECK(passed[wire::kName] == "score");
  CHECK(passed[wire::kData] == kPoints);
  CHECK_FALSE(passed.contains("extra"));
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

TEST_CASE("the peers of a welcome are ids there can be, each once, and fewer than a full game") {
  const auto welcome = [](const json& peers) { return json{{wire::kKind, wire::kWelcome}, {wire::kPeers, peers}}; };
  json others = json::array();
  for (int id = kHostId; id < kMaxPeers; ++id) others.push_back(id);
  CHECK(wire::well_formed(welcome(others)));  // everyone but the newcomer, in a full game
  others.push_back(kMaxPeers);
  CHECK_FALSE(wire::well_formed(welcome(others)));
  CHECK_FALSE(wire::well_formed(welcome({kHostId, kGuest, kGuest})));
  CHECK_FALSE(wire::well_formed(welcome({kHostId, 0})));
  CHECK_FALSE(wire::well_formed(welcome({kHostId, -kGuest})));
  CHECK_FALSE(wire::well_formed(welcome({kHostId, kMaxPeers + 1})));
  CHECK_FALSE(wire::well_formed(welcome({kHostId, kFarTooLargeId})));
  CHECK_FALSE(wire::well_formed(welcome(json::array({kHostId, kNotWhole}))));
}

TEST_CASE("an array or a hash holds only so many things") {
  CHECK(acceptable_data(json(std::vector<int>(kMaxDataMembers, 0))));
  CHECK_FALSE(acceptable_data(json(std::vector<int>(kMaxDataMembers + 1, 0))));
  json crowded = json::object();
  for (std::size_t i = 0; i <= kMaxDataMembers; ++i) crowded[std::to_string(i)] = true;
  CHECK_FALSE(acceptable_data(crowded));
  CHECK_FALSE(acceptable_data(json{{"inside", crowded}}));
  crowded.erase("0");
  CHECK(acceptable_data(crowded));
  CHECK_FALSE(wire::well_formed(
      json{{wire::kKind, wire::kGameMessage}, {wire::kData, std::vector<int>(kMaxDataMembers + 1, 0)}}));
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

TEST_CASE("a game's message is one of that kind with a name to hear it by") {
  CHECK(wire::is_game_message(wire::game_message("hit", nullptr, kHostId, kEveryone)));
  CHECK_FALSE(wire::is_game_message({{wire::kKind, wire::kGameMessage}}));
  CHECK_FALSE(wire::is_game_message({{wire::kKind, wire::kPing}, {wire::kName, "hit"}}));
}

TEST_CASE("a game's data is never bytes, nor a number that is not one") {
  CHECK(acceptable_data(json{{"x", 1.5}, {"big", std::numeric_limits<std::uint64_t>::max()}}));
  CHECK_FALSE(acceptable_data(json::binary({1, 2, 3})));
  CHECK_FALSE(acceptable_data(json::array({json::binary({1})})));
  for (const double odd : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
    CAPTURE(odd);
    CHECK_FALSE(acceptable_data(json(odd)));
    CHECK_FALSE(acceptable_data(json{{"deep", json::array({odd})}}));
    CHECK_FALSE(wire::well_formed(wire::game_message("odd", odd, kGuest, kEveryone)));
  }
}

TEST_CASE("the first words of a connection say the version of the protocol") {
  const json hello = wire::hello();
  CHECK(hello[wire::kKind] == wire::kHello);
  CHECK(wire::speaks_our_protocol(hello));
  CHECK(wire::refusal("full")[wire::kVersion] == wire::kProtocolVersion);
  CHECK(wire::protocol_of(json{{wire::kKind, wire::kHello}}) == 0);  // a hello that says none
  CHECK_FALSE(wire::speaks_our_protocol({{wire::kKind, wire::kHello}, {wire::kVersion, wire::kProtocolVersion + 1}}));
  CHECK(wire::protocol_mismatch(wire::kProtocolVersion + 1) ==
        "this game runs protocol " + std::to_string(wire::kProtocolVersion) + ", the other side " +
            std::to_string(wire::kProtocolVersion + 1));
}

TEST_CASE("a version is a whole number a version can be") {
  CHECK(wire::well_formed({{wire::kKind, wire::kHello}, {wire::kVersion, wire::kLargestProtocolVersion}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kHello}, {wire::kVersion, wire::kLargestProtocolVersion + 1}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kHello}, {wire::kVersion, -1}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kHello}, {wire::kVersion, "1"}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kHello}, {wire::kVersion, kNotWhole}}));
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

TEST_CASE("what a hello carries for the game to decide on is nothing, or a small hash of message data") {
  CHECK(wire::acceptable_auth(json()));
  CHECK(wire::acceptable_auth(json{{"token", "abc"}, {"name", "Ana"}}));
  CHECK(wire::acceptable_auth(json::object()));
  CHECK_FALSE(wire::acceptable_auth(json("abc")));
  CHECK_FALSE(wire::acceptable_auth(json::array({1, 2})));
  CHECK_FALSE(wire::acceptable_auth(json{{"token", std::string(kMaxAuthBytes, 'a')}}));
  json deep = json::object();
  for (int level = 0; level <= kMaxDataDepth; ++level) deep = json{{"in", deep}};
  CHECK_FALSE(wire::acceptable_auth(deep));
}

TEST_CASE("a refusal says why in a few words, and a hello with what a game decides on is a hello") {
  const json refused = {{wire::kKind, wire::kRefused}, {wire::kReason, "not logged in"}};
  CHECK(wire::well_formed(refused));
  CHECK_FALSE(
      wire::well_formed({{wire::kKind, wire::kRefused}, {wire::kReason, std::string(kMaxReasonBytes + 1, 'a')}}));
  CHECK_FALSE(wire::well_formed({{wire::kKind, wire::kRefused}, {wire::kReason, 7}}));
  // (What a hello carries is checked where it is decided on, so that the newcomer hears why.)
  CHECK(wire::well_formed({{wire::kKind, wire::kHello}, {wire::kAuth, {{"token", "abc"}}}}));
}
