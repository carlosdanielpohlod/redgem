#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "net/frame_buffer.hpp"
#include "net/pose_codec.hpp"
#include "net/wire.hpp"
#include "net/wire_codec.hpp"

using namespace redgem::net;
using nlohmann::json;

namespace {

// MessagePack's own bytes, for writing what no encoder would.
constexpr std::uint8_t kArrayOfOne = 0x91;
constexpr std::uint8_t kNil = 0xC0;
constexpr std::uint8_t kArray32 = 0xDD;  // an array, its count in the 4 bytes that follow
constexpr std::uint8_t kMap32 = 0xDF;    // a map, likewise
constexpr std::uint8_t kString8 = 0xD9;  // a text, its length in the byte that follows
constexpr std::uint8_t kAnnouncedLength = 200;
constexpr std::uint8_t kMost = 0xFF;
constexpr int kFarTooDeep = 100000;  // arrays in arrays: past any stack, were they read one inside the other
constexpr int kGuest = 2;
constexpr int kOther = 3;

std::string as_text(const std::vector<std::uint8_t>& bytes) { return {bytes.begin(), bytes.end()}; }

std::string msgpack_of(const json& message) { return as_text(json::to_msgpack(message)); }

// What the frame of a packed message holds: the bytes after its length.
std::string inside(const std::vector<std::uint8_t>& framed) {
  FrameBuffer buffer;
  buffer.append(framed.data(), framed.size());
  return buffer.next().value_or(std::string());
}

json shot() {
  return wire::game_message("shot",
                            {{"at", {{"$v", {1.5, -2.25, 1e6}}}},
                             {"power", 0.1},
                             {"who", "Ana \xE2\x98\x83"},  // (a snowman: text beyond ASCII)
                             {"tags", {1, true, nullptr, -7, "x"}}},
                            kGuest, kOther);
}

// Arrays in arrays, `depth` of them.
json nested(int depth) {
  json data = json::array();
  for (int i = 1; i < depth; ++i) data = json::array({data});
  return data;
}

json message_carrying(const json& data) { return wire::game_message("x", data, kGuest, kEveryone); }

}  // namespace

TEST_CASE("a message comes back from its bytes as it was sent") {
  for (const json& message : {shot(), wire::hello(), wire::refusal("full"), wire::pose_message({1, 2, 3}, kGuest),
                              json{{wire::kKind, wire::kWelcome}, {wire::kTo, kGuest}, {wire::kPeers, {1, kOther}}}}) {
    CAPTURE(message.dump());
    const auto back = wire::unpacked(inside(wire::packed(message)));
    REQUIRE(back.has_value());
    CHECK(*back == message);
    CHECK(wire::well_formed(*back));
  }
}

TEST_CASE("a message is far smaller in MessagePack than it was as JSON text") {
  const json message = shot();
  CHECK(wire::msgpack(message).size() < message.dump().size());
}

TEST_CASE("text that is not UTF-8 is sent with the broken bytes replaced, keys included") {
  const json broken = wire::game_message("caf\xE9", {{"k\xFF", "x\xC0\x80"}}, kGuest, kEveryone);
  const auto back = wire::unpacked(inside(wire::packed(broken)));
  REQUIRE(back.has_value());
  CHECK(wire::valid_utf8((*back)[wire::kName].get<std::string>()));
  CHECK((*back)[wire::kName] == "caf\xEF\xBF\xBD");
  CHECK(wire::valid_utf8((*back)[wire::kData].begin().key()));
}

TEST_CASE("bytes that arrive with text that is not UTF-8 are no message") {
  for (const std::string& bad : {std::string("caf\xE9"), std::string("\xC0\x80"), std::string("\xED\xA0\x80"),
                                 std::string("\xF4\x90\x80\x80"), std::string("cut \xE2\x98")}) {
    CAPTURE(bad);
    CHECK_FALSE(wire::valid_utf8(bad));
    CHECK_FALSE(wire::unpacked(msgpack_of({{wire::kKind, wire::kGameMessage}, {wire::kName, bad}})).has_value());
    CHECK_FALSE(wire::unpacked(msgpack_of({{wire::kKind, wire::kGameMessage}, {wire::kData, {{bad, 1}}}})).has_value());
  }
  CHECK(wire::valid_utf8("plain"));
  CHECK(wire::valid_utf8("Ana \xE2\x98\x83 \xF0\x9F\x8E\xAE"));
}

TEST_CASE("every part of a message cut short is no message, nor is one with more after it") {
  const std::string whole = msgpack_of(shot());
  for (std::size_t size = 0; size < whole.size(); ++size) {
    CAPTURE(size);
    CHECK_FALSE(wire::unpacked(whole.substr(0, size)).has_value());
  }
  CHECK_FALSE(wire::unpacked(whole + '\0').has_value());
  CHECK_FALSE(wire::unpacked("this is not a message").has_value());
  CHECK_FALSE(wire::unpacked(R"({"k":"i"})").has_value());  // JSON text, as versions before this one sent it
}

TEST_CASE("arrays nested past what a message may hold are refused as they are read, however deep") {
  std::string deep(kFarTooDeep, static_cast<char>(kArrayOfOne));
  deep += static_cast<char>(kNil);
  CHECK_FALSE(wire::unpacked(deep).has_value());  // (and the reader's stack is fine)
  // A message's data may nest kMaxDataDepth deep, inside the message itself, and no deeper.
  CHECK(wire::unpacked(msgpack_of(message_carrying(nested(kMaxDataDepth)))).has_value());
  CHECK_FALSE(wire::unpacked(msgpack_of(message_carrying(nested(kMaxDataDepth + 1)))).has_value());
}

TEST_CASE("an array or a map that says it holds more than one may is refused before any of it is read") {
  for (const std::uint8_t kind : {kArray32, kMap32}) {
    CAPTURE(static_cast<int>(kind));
    const std::string huge{static_cast<char>(kind), static_cast<char>(kMost), static_cast<char>(kMost),
                           static_cast<char>(kMost), static_cast<char>(kMost)};
    CHECK_FALSE(wire::unpacked(huge).has_value());
  }
  CHECK(wire::unpacked(msgpack_of(message_carrying(std::vector<int>(kMaxDataMembers, 1)))).has_value());
  CHECK_FALSE(wire::unpacked(msgpack_of(message_carrying(std::vector<int>(kMaxDataMembers + 1, 1)))).has_value());
  // A text longer than what follows it.
  CHECK_FALSE(
      wire::unpacked(std::string{static_cast<char>(kString8), static_cast<char>(kAnnouncedLength), 'a'}).has_value());
}

TEST_CASE("what arrives in the wrong shape is read, and then is not well formed") {
  for (const json& odd : {json::array({1, 2}), json("text"), json(kGuest), json{{wire::kKind, kGuest}},
                          message_carrying(json::binary({1, 2})), message_carrying(std::nan("")),
                          message_carrying(std::numeric_limits<double>::infinity()),
                          json{{wire::kKind, wire::kPose}, {wire::kData, "not bytes"}},
                          wire::pose_message(std::vector<std::uint8_t>(kMaxPoseBytes + 1), kGuest)}) {
    CAPTURE(odd.dump());
    const auto read = wire::unpacked(msgpack_of(odd));
    REQUIRE(read.has_value());
    CHECK_FALSE(wire::well_formed(*read));
  }
}

TEST_CASE("UTF-8 is read to the letter: every first byte, every range of the second, and every cut") {
  for (const std::string& good :
       {std::string(""), std::string("\x7F"), std::string("\xC2\x80"), std::string("\xDF\xBF"),
        std::string("\xE0\xA0\x80"), std::string("\xED\x9F\xBF"), std::string("\xEE\x80\x80"),
        std::string("\xEF\xBF\xBF"), std::string("\xF0\x90\x80\x80"), std::string("\xF4\x8F\xBF\xBF"),
        std::string("a\0b", 3)}) {
    CAPTURE(good);
    CHECK(wire::valid_utf8(good));
  }
  for (const std::string& bad :
       {std::string("\x80"), std::string("\xBF"), std::string("\xC0\xAF"), std::string("\xC1\xBF"), std::string("\xC2"),
        std::string("\xC2\x7F"), std::string("\xC2\xC0"), std::string("\xE0\x9F\xBF"), std::string("\xE0\xA0"),
        std::string("\xED\xA0\x80"), std::string("\xF0\x8F\xBF\xBF"), std::string("\xF4\x90\x80\x80"),
        std::string("\xF5\x80\x80\x80"), std::string("\xFF"), std::string("\xF0\x90\x80"), std::string("ok\xE2\x98")}) {
    CAPTURE(bad);
    CHECK_FALSE(wire::valid_utf8(bad));
  }
}
