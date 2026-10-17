#include <doctest/doctest.h>

#include <chrono>
#include <functional>
#include <optional>
#include <thread>

#include "integration/wire_text.hpp"
#include "net/connection.hpp"
#include "test_support.hpp"

using namespace redgem::net;
using redgem::test::raw_frame;
using redgem::test::wire_frame;

namespace {

constexpr int kMaxRounds = 3000;  // of pumping, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr const char* kLocalhost = "127.0.0.1";
constexpr std::size_t kBigTextBytes = 300 * 1024;  // more than one read or write takes
constexpr int kMoreThanAPoll = 3 * kMaxMessagesPerPoll / 2;
constexpr std::size_t kSmallPendingLimit = 256 * 1024;
constexpr std::size_t kUnreadTextBytes = 100 * 1024;
constexpr int kUnreadTexts = 400;        // far more than a connection nobody reads from takes
constexpr float kQuietStep = 4.0f;       // seconds: shorter than the time between "I am here"s
constexpr int kPingsBeforeGivingUp = 3;  // one every third of those steps, until the silence is too long
constexpr float kPastJoinTimeout = kJoinTimeoutSeconds + 1.0f;
constexpr std::size_t kLengthBytes = 4;
// The start of a message longer than any may be.
constexpr std::uint8_t kTooLongAnnounced[kLengthBytes] = {0x7F, 0x00, 0x00, 0x00};
constexpr const char* kTinyMessage = R"({"k":"m","n":"x"})";
constexpr float kArrivalWaitSeconds = 3.0f;  // far longer than a message on this machine takes

bool until(const std::function<bool()>& done) {
  for (int round = 0; round < kMaxRounds; ++round) {
    if (done()) return true;
    std::this_thread::sleep_for(kRoundPause);
  }
  return false;
}

// Two ends of one connection on this machine: `caller` connected out, `taker` was connected to.
struct BothEnds {
  Socket listener;
  Connection caller, taker;

  explicit BothEnds(std::size_t taker_pending_limit = kMaxPendingBytes) {
    std::string error;
    listener = Socket::listen_on(0, error);
    REQUIRE_MESSAGE(listener.valid(), error);
    caller = Connection::calling(Socket::connect_to(kLocalhost, listener.local_port(), error));
    Socket taken;
    REQUIRE(until([&] { return listener.accept(taken) == IoStatus::Ok; }));
    taker = Connection::accepted(std::move(taken), taker_pending_limit);
    // (Each side's first words say the protocol's version, as a connection's always do.)
    caller.send(wire::hello());
    taker.send(wire::hello());
    REQUIRE(heard_by(taker, 1).size() == 1);
    REQUIRE(heard_by(caller, 1).size() == 1);
  }

  // The messages `to` takes, pumping both ends until there are `wanted` of them.
  std::vector<nlohmann::json> heard_by(Connection& to, std::size_t wanted) {
    std::vector<nlohmann::json> heard;
    until([&] {
      caller.pump();
      taker.pump();
      while (const auto message = to.next_message()) heard.push_back(*message);
      return heard.size() >= wanted;
    });
    return heard;
  }
};

// One end as a Connection, the other a plain socket that says whatever it likes.
struct WithStranger {
  Socket listener, stranger;
  Connection taker;
  int pumps = 0;

  WithStranger() {
    std::string error;
    listener = Socket::listen_on(0, error);
    REQUIRE_MESSAGE(listener.valid(), error);
    stranger = Socket::connect_to(kLocalhost, listener.local_port(), error);
    Socket taken;
    REQUIRE(until([&] { return listener.accept(taken) == IoStatus::Ok; }));
    taker = Connection::accepted(std::move(taken));
    REQUIRE(until([this] { return stranger.connect_status() == IoStatus::Ok; }));
  }

  // A stranger that has said its first words already, as the tests below need.
  static WithStranger greeted() {
    WithStranger pair;
    pair.say_hello();
    return pair;
  }

  // The stranger's first words: a hello of this version, taken.
  void say_hello() {
    const std::vector<std::uint8_t> hello = wire_frame(R"({"k":"h"})");
    write(hello);
    take_in(hello.size());
    REQUIRE(taker.next_message().has_value());
  }

  // What the taker sent the stranger, read whole.
  nlohmann::json heard_by_stranger() {
    FrameBuffer heard;
    std::optional<std::string> bytes;
    until([&] {
      taker.pump();
      std::vector<std::uint8_t> arrived;
      stranger.read(arrived, kBigTextBytes);
      heard.append(arrived.data(), arrived.size());
      bytes = heard.next();
      return bytes.has_value();
    });
    return bytes ? redgem::test::message_in(*bytes) : nlohmann::json();
  }

  void write(std::vector<std::uint8_t> bytes) {
    REQUIRE(until([&] { return stranger.write(bytes) == IoStatus::Ok; }));
  }

  // Takes messages until the stranger is seen to have hung up; how many there were.
  int messages_until_gone() {
    int taken = 0;
    REQUIRE(until([&] {
      taker.pump();
      ++pumps;
      while (taker.next_message()) ++taken;
      return taker.closed();
    }));
    return taken;
  }

  // Pumps until `bytes` more have arrived (or the other side is seen to have hung up).
  void take_in(std::size_t bytes) {
    std::size_t arrived = 0;
    REQUIRE(until([&] {
      arrived += taker.pump();
      return arrived >= bytes;
    }));
  }
};

std::vector<std::uint8_t> many_tiny_messages(int count) {
  std::vector<std::uint8_t> bytes;
  const std::vector<std::uint8_t> tiny = wire_frame(kTinyMessage);
  for (int i = 0; i < count; ++i) bytes.insert(bytes.end(), tiny.begin(), tiny.end());
  return bytes;
}

}  // namespace

TEST_CASE("what one end of a connection sends the other takes, whole and in order") {
  BothEnds ends;
  // (Said while still connecting: it goes out once the connection is made.)
  ends.caller.send({{"k", "m"}, {"n", "first"}, {"d", std::string(kBigTextBytes, 'x')}});
  ends.caller.send({{"k", "m"}, {"n", "second"}});
  const std::vector<nlohmann::json> heard = ends.heard_by(ends.taker, 2);
  REQUIRE(heard.size() == 2);
  CHECK(heard[0]["n"] == "first");
  CHECK(heard[0]["d"].get<std::string>().size() == kBigTextBytes);
  CHECK(heard[1]["n"] == "second");
  ends.taker.send({{"k", "m"}, {"n", "back"}});
  REQUIRE(ends.heard_by(ends.caller, 1).size() == 1);
  CHECK_FALSE(ends.caller.closed());
  CHECK_FALSE(ends.taker.closed());
  CHECK(ends.taker.address() == kLocalhost);
}

TEST_CASE("a connection knows what it holds, and what a wait should watch it for") {
  WithStranger pair = WithStranger::greeted();
  CHECK(pair.taker.held() == 0);
  CHECK_FALSE(pair.taker.message_waiting());
  CHECK_FALSE(pair.taker.watched().sending);
  const std::vector<std::uint8_t> tiny = wire_frame(kTinyMessage);
  pair.write(tiny);
  pair.take_in(tiny.size());
  CHECK(pair.taker.held() == tiny.size());
  CHECK(pair.taker.message_waiting());
  REQUIRE(pair.taker.next_message().has_value());
  CHECK(pair.taker.held() == 0);
  CHECK_FALSE(pair.taker.message_waiting());
}

TEST_CASE("a call nobody answers is a closed connection") {
  std::string error;
  Socket listener = Socket::listen_on(0, error);
  const std::uint16_t port = listener.local_port();
  listener.close();
  Connection caller = Connection::calling(Socket::connect_to(kLocalhost, port, error));
  caller.send({{"k", "h"}});
  CHECK(until([&caller] {
    caller.pump();
    return caller.closed();
  }));
  CHECK(caller.held() == 0);
}

TEST_CASE("what is not a message of ours closes the connection") {
  // (A message as JSON text, as a version of the engine before MessagePack sent it, is not one either.)
  for (const std::vector<std::uint8_t>& bytes :
       {raw_frame("this is not a message"), raw_frame(kTinyMessage), wire_frame(R"({"k":7})"),
        wire_frame(R"({"k":"m","n":"x","t":"everyone"})")}) {
    WithStranger pair = WithStranger::greeted();
    pair.write(bytes);
    pair.take_in(bytes.size());
    CHECK_FALSE(pair.taker.next_message().has_value());
    CHECK(pair.taker.closed());
  }
  WithStranger pair = WithStranger::greeted();
  pair.write({std::begin(kTooLongAnnounced), std::end(kTooLongAnnounced)});
  pair.take_in(kLengthBytes);
  CHECK_FALSE(pair.taker.next_message().has_value());
  CHECK(pair.taker.closed());
}

TEST_CASE("only a poll's worth of messages is taken between two pumps; the rest wait for the next") {
  WithStranger pair = WithStranger::greeted();
  const std::vector<std::uint8_t> flood = many_tiny_messages(kMoreThanAPoll);
  pair.write(flood);
  pair.take_in(flood.size());
  int taken = 0;
  while (pair.taker.next_message()) ++taken;
  CHECK(taken == kMaxMessagesPerPoll);
  CHECK(pair.taker.message_waiting());
  pair.taker.pump();
  while (pair.taker.next_message()) ++taken;
  CHECK(taken == kMoreThanAPoll);
}

TEST_CASE("what a machine said before it hung up is still taken, however many polls that needs") {
  WithStranger pair = WithStranger::greeted();
  const std::vector<std::uint8_t> last_words = many_tiny_messages(kMoreThanAPoll);
  pair.write(last_words);
  pair.stranger.close();
  CHECK(pair.messages_until_gone() == kMoreThanAPoll);  // none was lost to the hanging up
  CHECK(pair.pumps > 1);                                // and no poll took more than its share
  // Nothing is said to a machine that is gone.
  pair.taker.send({{"k", "i"}});
  CHECK(pair.taker.held() == 0);
}

// A connection closed with unread bytes in it ends with a reset instead of a goodbye, and on
// Windows a reset throws away what the other side had not read yet.
TEST_CASE("a machine that hangs up without reading what it was sent still has all it said taken") {
  WithStranger pair = WithStranger::greeted();
  pair.taker.send({{"k", "i"}});
  REQUIRE(Socket::wait({Watched{&pair.stranger}}, kArrivalWaitSeconds));  // there, and never read
  pair.write(many_tiny_messages(kMoreThanAPoll));
  pair.stranger.close();
  CHECK(pair.messages_until_gone() == kMoreThanAPoll);
}

TEST_CASE("half a message from a machine that hung up is nothing to wait for") {
  WithStranger pair = WithStranger::greeted();
  std::vector<std::uint8_t> half = wire_frame(kTinyMessage);
  half.pop_back();
  pair.write(half);
  pair.stranger.close();
  CHECK(until([&pair] {
    pair.taker.pump();
    return pair.taker.closed();
  }));
  CHECK_FALSE(pair.taker.next_message().has_value());
}

TEST_CASE("a connection that is closed forgets what was not taken, and hangs up") {
  WithStranger pair = WithStranger::greeted();
  const std::vector<std::uint8_t> words = many_tiny_messages(2);
  pair.write(words);
  pair.take_in(words.size());
  pair.taker.close();
  CHECK(pair.taker.closed());
  CHECK_FALSE(pair.taker.next_message().has_value());
  CHECK(pair.taker.held() == 0);
  std::vector<std::uint8_t> ignored;
  CHECK(until([&] { return pair.stranger.read(ignored, kBigTextBytes) == IoStatus::Closed; }));
}

TEST_CASE("a machine that stopped listening is let go once too much waits to be sent to it") {
  BothEnds ends(kSmallPendingLimit);
  const nlohmann::json text = {{"k", "m"}, {"n", "x"}, {"d", std::string(kUnreadTextBytes, 'a')}};
  ends.taker.send(text);
  CHECK_FALSE(ends.taker.closed());  // one message waiting is no reason
  int sent = 1;
  for (; sent < kUnreadTexts && !ends.taker.closed(); ++sent) ends.taker.send(text);  // (the caller never pumps)
  CHECK(ends.taker.closed());
  CHECK(sent < kUnreadTexts);
  CHECK(ends.taker.held() == 0);
}

TEST_CASE("a quiet connection says it is there; a silent one is given up on") {
  BothEnds ends;
  // The taker hears nothing but keeps saying "I am here": the caller, which hears that, stays.
  for (float waited = 0.0f; waited < kSilenceTimeoutSeconds + kQuietStep; waited += kQuietStep) {
    ends.taker.keep_in_touch(kQuietStep);
    std::this_thread::sleep_for(kRoundPause);
    ends.caller.pump();
    ends.caller.keep_in_touch(0.0f);
  }
  CHECK(ends.taker.closed());  // nothing ever came from the caller
  int pings = 0;
  while (const auto message = ends.caller.next_message()) pings += (*message)["k"] == "i" ? 1 : 0;
  CHECK(pings == kPingsBeforeGivingUp);
}

TEST_CASE("a newcomer that never says what it wants is given up on") {
  WithStranger pair = WithStranger::greeted();
  pair.taker.keep_waiting(kQuietStep);
  CHECK_FALSE(pair.taker.closed());
  pair.taker.keep_waiting(kPastJoinTimeout);
  CHECK(pair.taker.closed());
}

TEST_CASE("first words of another version are refused with why, though this version could not read the rest") {
  // A newer version's hello may carry what this one's well_formed refuses (a kind of list it does not know).
  redgem::test::LogCapture log;
  WithStranger pair;
  const int next_protocol = wire::kProtocolVersion + 1;
  const std::vector<std::uint8_t> hello =
      wire_frame(R"({"k":"h","p":"not a list","z":[1],"v":)" + std::to_string(next_protocol) + "}");
  pair.write(hello);
  pair.take_in(hello.size());
  CHECK_FALSE(pair.taker.next_message().has_value());
  const nlohmann::json refusal = pair.heard_by_stranger();
  CHECK(refusal[wire::kKind] == wire::kRefused);
  CHECK(refusal[wire::kReason] == wire::protocol_mismatch(next_protocol));
  CHECK(pair.taker.closed());
  CHECK(log.contains(wire::protocol_mismatch(next_protocol)));
}

TEST_CASE("to a machine that called, the first words of another version are a refusal saying that version") {
  BothEnds ends;  // (their hellos were taken: what follows is not first words)
  std::string error;
  Socket listener = Socket::listen_on(0, error);
  Connection caller = Connection::calling(Socket::connect_to(kLocalhost, listener.local_port(), error));
  Socket host;
  REQUIRE(until([&] { return listener.accept(host) == IoStatus::Ok; }));
  std::vector<std::uint8_t> welcome = wire_frame(R"({"k":"w","t":"two","v":7})");
  REQUIRE(until([&] { return host.write(welcome) == IoStatus::Ok && welcome.empty(); }));
  std::optional<nlohmann::json> heard;
  REQUIRE(until([&] {
    caller.pump();
    heard = caller.next_message();
    return heard.has_value();
  }));
  CHECK((*heard)[wire::kKind] == wire::kRefused);
  CHECK(wire::protocol_of(*heard) == 7);
  // A message after the first is read as this version's, and in the wrong shape closes the connection.
  ends.taker.send({{wire::kKind, wire::kGameMessage}, {wire::kName, "x"}, {wire::kTo, "two"}});
  CHECK(until([&ends] {
    ends.taker.pump();
    ends.caller.pump();
    ends.caller.next_message();
    return ends.caller.closed();
  }));
}
