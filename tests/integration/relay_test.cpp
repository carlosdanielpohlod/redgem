#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <set>
#include <thread>

#ifndef _WIN32
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "core/platform.hpp"
#include "integration/wire_text.hpp"
#include "net/discovery/lan_discovery.hpp"
#include "net/network_session.hpp"
#include "net/rooms/relay_program.hpp"
#include "net/rooms/relay_server.hpp"
#include "test_support.hpp"

using namespace redgem::net;
using redgem::test::wire_frame;

namespace {

constexpr float kShortWaitSeconds = 0.05f;
constexpr std::chrono::milliseconds kShortWait{50};
// Windows can end a timed wait up to one tick of its system timer (about 15.6 ms) early.
constexpr std::chrono::milliseconds kClockTick{16};
constexpr float kLongWaitSeconds = 5.0f;  // far longer than anything here takes to arrive
constexpr std::chrono::milliseconds kLongWait{2500};
constexpr std::chrono::milliseconds kPromptly{500};  // well under the second an idle relay sleeps
constexpr int kMaxRounds = 3000;                     // of polling, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr float kRoundSeconds = 0.001f;
constexpr float kPastJoinTimeout = 6.0f;
constexpr float kPastForgetting = 4.0f;  // seconds after which a silent game is forgotten
constexpr int kSettleRounds = 50;
constexpr int kTooManyGuesses = 2 * kMaxPeers + 2;  // more wrong room codes than one address is allowed
constexpr int kPartyGuests = kMaxPeers - 1;         // everyone a room has places for besides its opener
constexpr int kBroadcasts = 4;
constexpr std::size_t kBroadcastBytes = 256 * 1024;
constexpr int kRoomsPerAddress = 8;
constexpr float kPastKeptOut = 125.0f;  // seconds an address stays out for guessing
constexpr float kQuietStep = 4.0f;
constexpr const char* kLocalhost = "127.0.0.1";
constexpr std::uint16_t kTestDiscoveryPort = 47831;  // not the real one: a game may be open on this machine
constexpr std::uint16_t kGamePort = 7777;
constexpr std::size_t kWelcomeBytes = 4096;  // plenty for a relay's first answer
constexpr int kManyCodes = 500;
constexpr std::string_view kCodeLetters = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr int kFloodMessages = 3 * kMaxMessagesPerPoll / 2;
constexpr std::chrono::milliseconds kArrivalPause{50};  // for what was written to be there to read
constexpr std::size_t kLongTextBytes = 512 * 1024;
constexpr int kLongTexts = 10;                     // together, more than a machine may send through a relay at once
constexpr std::size_t kSmallBudget = 1024 * 1024;  // what a small relay holds for all its machines
constexpr std::size_t kLengthBytes = 4;
// The start of a message most of a small relay's budget long, the rest of which never comes.
constexpr std::uint8_t kLongAnnounced[kLengthBytes] = {0x00, 0x0E, 0x00, 0x00};
constexpr std::size_t kHalfSentBytes = 600 * 1024;

// A relay and the sessions talking through it, with everything that happened to each.
struct Meeting {
  RelayServer relay;
  std::vector<NetworkSession*> sessions;
  std::vector<std::vector<NetworkEvent>> events;

  explicit Meeting(std::size_t buffered_bytes = kMaxRelayBufferedBytes) : relay(buffered_bytes) { start(); }
  explicit Meeting(const RelayLimits& limits) : relay(limits) { start(); }

  void start() {
    std::string error;
    REQUIRE_MESSAGE(relay.start(0, error), error);
  }

  void seat(NetworkSession& session) {
    sessions.push_back(&session);
    events.emplace_back();
  }

  bool until(const std::function<bool()>& done) {
    for (int round = 0; round < kMaxRounds; ++round) {
      relay.poll(kRoundSeconds);
      for (std::size_t i = 0; i < sessions.size(); ++i) {
        for (NetworkEvent& event : sessions[i]->poll(kRoundSeconds)) events[i].push_back(std::move(event));
      }
      if (done()) return true;
      std::this_thread::sleep_for(kRoundPause);
    }
    return false;
  }

  int count(std::size_t seat_index, NetworkEvent::Kind kind) const {
    int total = 0;
    for (const NetworkEvent& event : events[seat_index]) total += event.kind == kind ? 1 : 0;
    return total;
  }

  const NetworkEvent* last(std::size_t seat_index, NetworkEvent::Kind kind) const {
    for (auto event = events[seat_index].rbegin(); event != events[seat_index].rend(); ++event) {
      if (event->kind == kind) return &*event;
    }
    return nullptr;
  }
};

// A room with the machine that opened it and two that joined.
struct RoomOfThree {
  Meeting meeting;
  NetworkSession opener, second, third;
  static constexpr std::size_t kOpenerSeat = 0, kSecondSeat = 1, kThirdSeat = 2;

  RoomOfThree() {
    std::string error;
    meeting.seat(opener);
    meeting.seat(second);
    meeting.seat(third);
    REQUIRE_MESSAGE(opener.host_room(kLocalhost, meeting.relay.port(), error), error);
    REQUIRE(meeting.until([this] { return opener.state() == NetworkSession::State::Online; }));
    for (NetworkSession* guest : {&second, &third}) {
      REQUIRE_MESSAGE(guest->join_room(kLocalhost, meeting.relay.port(), opener.room(), error), error);
      REQUIRE(meeting.until([guest] { return guest->state() == NetworkSession::State::Online; }));
    }
    REQUIRE(meeting.until([this] { return opener.peers().size() == 2 && second.peers().size() == 2; }));
  }
};

// Something that is not this engine, talking to a relay over a plain connection.
struct RawMachine {
  Meeting& meeting;
  Socket socket;
  FrameBuffer heard;

  // (Connected before anything is written: a Mac refuses to send on a connection still in the making.)
  explicit RawMachine(Meeting& where) : meeting(where) {
    std::string error;
    socket = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
    REQUIRE(socket.valid());
    REQUIRE(meeting.until([this] { return socket.connect_status() == IoStatus::Ok; }));
  }

  // Writes all of `bytes`; false when the relay hung up first.
  bool send(std::vector<std::uint8_t> bytes) {
    IoStatus status = IoStatus::Ok;
    meeting.until([&] {
      status = socket.write(bytes);
      return bytes.empty() || status == IoStatus::Closed;
    });
    return bytes.empty();
  }

  bool say(const std::string& text) { return send(wire_frame(text)); }

  IoStatus listen() {
    std::vector<std::uint8_t> arrived;
    const IoStatus status = socket.read(arrived, kMaxReadBytesPerPoll);
    heard.append(arrived.data(), arrived.size());
    return status;
  }

  // The next message the relay sent, waited for.
  nlohmann::json next_message() {
    std::optional<std::string> text;
    REQUIRE(meeting.until([&] {
      listen();
      text = heard.next();
      return text.has_value();
    }));
    return redgem::test::message_in(*text);
  }

  // Opens a room and returns its code.
  std::string open_room() {
    REQUIRE(say(R"({"k":"h","r":"new"})"));
    return next_message()["r"];
  }
};

// An answer to a search, written as JSON text, as the bytes that travel (MessagePack).
std::string answer(const std::string& text) {
  const std::vector<std::uint8_t> bytes = nlohmann::json::to_msgpack(nlohmann::json::parse(text));
  return {bytes.begin(), bytes.end()};
}

const std::string kVersionField = R"("v":)" + std::to_string(wire::kProtocolVersion);

// What a stranger answers a search with: nonsense, half answers, answers from no game of this
// engine, an answer as JSON text (as versions before MessagePack sent it) and one of another
// version of the protocol, then one real answer.
const std::vector<std::string> kNotAnswers = {
    "hello",
    answer(R"({"is":"redgem"})"),
    answer(R"({"is":"redgem","name":7,"port":7777,"players":1,)" + kVersionField + "}"),
    answer(R"({"is":"redgem","name":"x","port":99999,"players":1,)" + kVersionField + "}"),
    answer(R"({"is":"other","name":"x","port":7777,"players":1,)" + kVersionField + "}"),
    answer(R"({"is":"redgem","name":"x","port":7777,"players":1})"),
    R"({"is":"redgem","name":"x","port":7777,"players":1,)" + kVersionField + "}",
    answer(R"({"is":"redgem","name":"newer","port":7779,"players":1,"v":)" +
           std::to_string(wire::kProtocolVersion + 1) + "}")};
const std::string kRealAnswer =
    answer(R"({"is":"redgem","name":"real","port":7777,"players":2,)" + kVersionField + "}");

// Answers every search that reached `listener` so far with all of the above. Every one, not just
// the first: the first to arrive may be one a finder of the test before sent as it was closed (the
// system can hand a datagram over a little after it was sent), whose port nobody listens on now.
void answer_with_nonsense(Datagrams& listener) {
  std::string question, from;
  std::uint16_t from_port = 0;
  while (listener.receive(question, from, from_port)) {
    for (const std::string& bad : kNotAnswers) listener.send_to(kLocalhost, from_port, bad);
    listener.send_to(kLocalhost, from_port, kRealAnswer);
  }
}

// Whether a wait of kShortWait lasted it, give or take the tick the system's timer may cut off.
bool slept_the_short_wait(std::chrono::steady_clock::duration slept) { return slept + kClockTick >= kShortWait; }

}  // namespace

TEST_CASE("an idle relay sleeps for as long as it is told when nothing arrives") {
  Meeting meeting;
  const auto before = std::chrono::steady_clock::now();
  CHECK_FALSE(meeting.relay.wait(kShortWaitSeconds));
  const auto slept = std::chrono::steady_clock::now() - before;
  CHECK(slept_the_short_wait(slept));
  CHECK(slept < kLongWait);
}

TEST_CASE("a relay's wait ends as soon as someone connects") {
  Meeting meeting;
  Socket caller;
  std::thread late([&] {
    std::this_thread::sleep_for(kArrivalPause);
    std::string error;
    caller = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
  });
  const auto before = std::chrono::steady_clock::now();
  const bool woken = meeting.relay.wait(kLongWaitSeconds);
  const auto slept = std::chrono::steady_clock::now() - before;
  late.join();
  CHECK(woken);
  CHECK(slept >= kArrivalPause);  // it slept until the call, not through it
  CHECK(slept < kLongWait);
  meeting.relay.poll(kRoundSeconds);
  CHECK(meeting.relay.connection_count() == 1);
}

TEST_CASE("a relay's wait ends when a machine it knows says something, or hangs up") {
  Meeting meeting;
  RawMachine machine(meeting);
  REQUIRE(meeting.until([&] { return meeting.relay.connection_count() == 1; }));
  // Connected and silent: only the time ends the wait.
  CHECK_FALSE(meeting.relay.wait(kShortWaitSeconds));
  std::vector<std::uint8_t> words = wire_frame(R"({"k":"h","r":"new"})");
  REQUIRE(machine.socket.write(words) == IoStatus::Ok);
  CHECK(meeting.relay.wait(kLongWaitSeconds));
  meeting.relay.poll(kRoundSeconds);
  CHECK(meeting.relay.room_count() == 1);
  // What the relay answered has gone out: it sleeps again, until the machine hangs up.
  CHECK_FALSE(meeting.relay.wait(kShortWaitSeconds));
  machine.socket.close();
  CHECK(meeting.relay.wait(kLongWaitSeconds));
  meeting.relay.poll(kRoundSeconds);
  CHECK(meeting.relay.connection_count() == 0);
}

TEST_CASE("a relay with messages still to handle does not sleep on them") {
  Meeting meeting;
  RawMachine opener(meeting), flooder(meeting);
  const std::string code = opener.open_room();
  REQUIRE(flooder.say(R"({"k":"h","r":")" + code + R"("})"));
  CHECK(opener.next_message()["k"] == "j");
  // More messages at once than one poll handles: the rest wait inside the relay, not in a socket.
  std::vector<std::uint8_t> flood;
  const std::vector<std::uint8_t> tiny = wire_frame(R"({"k":"m","n":"x"})");
  for (int i = 0; i < kFloodMessages; ++i) flood.insert(flood.end(), tiny.begin(), tiny.end());
  while (!flood.empty()) REQUIRE(flooder.socket.write(flood) != IoStatus::Closed);
  std::this_thread::sleep_for(kArrivalPause);
  meeting.relay.poll(kRoundSeconds);
  const auto before = std::chrono::steady_clock::now();
  CHECK(meeting.relay.wait(kLongWaitSeconds));
  CHECK(std::chrono::steady_clock::now() - before < kShortWait);
}

TEST_CASE("a wait on no socket at all is a plain sleep, and a wait of no time does not sleep") {
  const auto before = std::chrono::steady_clock::now();
  CHECK_FALSE(Socket::wait({}, kShortWaitSeconds));
  CHECK(slept_the_short_wait(std::chrono::steady_clock::now() - before));
  const Socket closed;
  CHECK_FALSE(Socket::wait({{&closed, true}, {nullptr, false}}, 0.0f));
  Meeting meeting;
  const auto again = std::chrono::steady_clock::now();
  CHECK_FALSE(meeting.relay.wait(0.0f));
  CHECK_FALSE(meeting.relay.wait(-1.0f));  // (never "for ever")
  CHECK(std::chrono::steady_clock::now() - again < kShortWait);
}

TEST_CASE("the relay program's turns sleep until there is work, then do it") {
  Meeting meeting;
  std::atomic<bool> stopped{false};
  // (Only this thread touches the relay while it runs.)
  std::thread program([&] {
    while (!stopped) serve_relay_once(meeting.relay, kRoundSeconds);
  });
  NetworkSession opener;
  std::string error;
  REQUIRE_MESSAGE(opener.host_room(kLocalhost, meeting.relay.port(), error), error);
  const auto before = std::chrono::steady_clock::now();
  for (int round = 0; round < kMaxRounds && opener.state() != NetworkSession::State::Online; ++round) {
    opener.poll(kRoundSeconds);
    std::this_thread::sleep_for(kRoundPause);
  }
  const auto took = std::chrono::steady_clock::now() - before;
  stopped = true;
  program.join();
  CHECK(opener.state() == NetworkSession::State::Online);
  // Each turn may sleep a second: a room opened in a fraction of one means arrivals ended the sleeps.
  CHECK(took < kPromptly);
  CHECK(meeting.relay.room_count() == 1);
}

TEST_CASE("a room's code is letters easy to read aloud, and no two rooms get the same") {
  std::set<std::string> codes;
  for (int i = 0; i < kManyCodes; ++i) {
    const std::string code = random_room_code();
    CHECK(code.size() == kRoomCodeLetters);
    CHECK(code.find_first_not_of(kCodeLetters) == std::string::npos);
    codes.insert(code);
  }
  CHECK(codes.size() == kManyCodes);
}

TEST_CASE("a flood of tiny messages through a relay is passed on a poll's worth at a time") {
  Meeting meeting;
  RawMachine opener(meeting), flooder(meeting);
  const std::string code = opener.open_room();
  REQUIRE(flooder.say(R"({"k":"h","r":")" + code + R"("})"));
  CHECK(opener.next_message()["k"] == "j");
  std::vector<std::uint8_t> flood;
  const std::vector<std::uint8_t> tiny = wire_frame(R"({"k":"m","n":"x"})");
  for (int i = 0; i < kFloodMessages; ++i) flood.insert(flood.end(), tiny.begin(), tiny.end());
  std::vector<std::uint8_t> unsent = flood;
  while (!unsent.empty()) REQUIRE(flooder.socket.write(unsent) != IoStatus::Closed);
  std::this_thread::sleep_for(kArrivalPause);
  meeting.relay.poll(kRoundSeconds);
  std::this_thread::sleep_for(kArrivalPause);
  opener.listen();
  int passed_on = 0;
  while (opener.heard.next()) ++passed_on;
  CHECK(passed_on == kMaxMessagesPerPoll);
  // The rest comes with the next polls: nothing is lost.
  REQUIRE(meeting.until([&] {
    opener.listen();
    while (opener.heard.next()) ++passed_on;
    return passed_on == kFloodMessages;
  }));
}

TEST_CASE("a machine that sends more than any game would is let go by the relay") {
  Meeting meeting;
  RawMachine flooder(meeting);
  flooder.open_room();
  REQUIRE(meeting.relay.room_count() == 1);
  const std::string long_text = R"({"k":"i","d":")" + std::string(kLongTextBytes, 'a') + R"("})";
  for (int i = 0; i < kLongTexts; ++i) flooder.say(long_text);
  REQUIRE(meeting.until([&meeting] { return meeting.relay.connection_count() == 0; }));
  CHECK(meeting.relay.room_count() == 0);
}

// Each message is work for the relay whatever its size: tiny ones, far more than a game sends,
// are a flood too, though their bytes alone are well within what a machine may send.
TEST_CASE("a machine that sends far more tiny messages than any game would is let go by the relay") {
  constexpr int kTinyFlood = 80000;  // ~1.7 MB of bytes; with what each message costs, past the burst
  Meeting meeting;
  RawMachine opener(meeting), flooder(meeting);
  const std::string code = opener.open_room();
  REQUIRE(flooder.say(R"({"k":"h","r":")" + code + R"("})"));
  CHECK(opener.next_message()["k"] == "j");
  std::vector<std::uint8_t> unsent;
  const std::vector<std::uint8_t> tiny = wire_frame(R"({"k":"m","n":"x"})");
  for (int i = 0; i < kTinyFlood; ++i) unsent.insert(unsent.end(), tiny.begin(), tiny.end());
  const auto flooding = [&] { return !unsent.empty() && meeting.relay.connection_count() == 2; };
  while (flooding() && flooder.socket.write(unsent) != IoStatus::Closed) meeting.relay.poll(kRoundSeconds);
  REQUIRE(meeting.until([&meeting] { return meeting.relay.connection_count() == 1; }));
  CHECK(meeting.relay.room_count() == 1);  // the opener plays on
}

TEST_CASE("a relay holding more than it may lets go of the newest machine with much waiting") {
  Meeting meeting(kSmallBudget);
  std::vector<std::uint8_t> half_sent(std::begin(kLongAnnounced), std::end(kLongAnnounced));
  half_sent.resize(kLengthBytes + kHalfSentBytes, 'a');
  RawMachine modest(meeting), first(meeting);
  modest.open_room();
  REQUIRE(first.send(half_sent));
  for (int round = 0; round < kSettleRounds; ++round) meeting.relay.poll(kRoundSeconds);
  CHECK(meeting.relay.connection_count() == 2);  // within what the relay may hold
  RawMachine second(meeting);
  second.send(half_sent);
  REQUIRE(meeting.until([&second] { return second.listen() == IoStatus::Closed; }));
  CHECK(meeting.relay.connection_count() == 2);
  CHECK(first.listen() == IoStatus::WouldBlock);   // whoever was there before stays ...
  CHECK(modest.listen() == IoStatus::WouldBlock);  // ... and so does whoever holds little
  CHECK(meeting.relay.room_count() == 1);
}

TEST_CASE("a room on a relay has a code, and whoever opened it is the game's host") {
  RoomOfThree room;
  CHECK(room.opener.room().size() == kRoomCodeLetters);
  CHECK(room.second.room() == room.opener.room());
  CHECK(room.opener.hosting());
  CHECK(room.opener.id() == kHostId);
  CHECK_FALSE(room.second.hosting());
  CHECK(room.second.id() == kFirstGuestId);
  CHECK(room.third.id() == kFirstGuestId + 1);
  CHECK(room.opener.peers() == std::vector<int>{kFirstGuestId, kFirstGuestId + 1});
  CHECK(room.third.peers() == std::vector<int>{kHostId, kFirstGuestId});
  CHECK(room.meeting.relay.room_count() == 1);
  CHECK(room.meeting.count(RoomOfThree::kOpenerSeat, NetworkEvent::Kind::Connected) == 1);
  CHECK(room.meeting.count(RoomOfThree::kOpenerSeat, NetworkEvent::Kind::PeerJoined) == 2);
}

TEST_CASE("messages in a room reach everyone else, or the one they are for, with who sent them") {
  RoomOfThree room;
  CHECK(room.opener.send("start", {{"level", 2}}));
  CHECK(room.second.send("whisper", "psst", kFirstGuestId + 1));
  CHECK(room.third.send("report", 7, kHostId));
  REQUIRE(room.meeting.until([&room] {
    return room.meeting.count(RoomOfThree::kOpenerSeat, NetworkEvent::Kind::Message) == 1 &&
           room.meeting.count(RoomOfThree::kSecondSeat, NetworkEvent::Kind::Message) == 1 &&
           room.meeting.count(RoomOfThree::kThirdSeat, NetworkEvent::Kind::Message) == 2;
  }));
  CHECK(room.meeting.last(RoomOfThree::kSecondSeat, NetworkEvent::Kind::Message)->name == "start");
  CHECK(room.meeting.last(RoomOfThree::kSecondSeat, NetworkEvent::Kind::Message)->peer == kHostId);
  CHECK(room.meeting.last(RoomOfThree::kSecondSeat, NetworkEvent::Kind::Message)->data["level"] == 2);
  CHECK(room.meeting.last(RoomOfThree::kOpenerSeat, NetworkEvent::Kind::Message)->name == "report");
  CHECK(room.meeting.last(RoomOfThree::kOpenerSeat, NetworkEvent::Kind::Message)->peer == kFirstGuestId + 1);
}

TEST_CASE("rooms are apart from each other") {
  RoomOfThree room;
  NetworkSession elsewhere;
  room.meeting.seat(elsewhere);
  std::string error;
  REQUIRE(elsewhere.host_room(kLocalhost, room.meeting.relay.port(), error));
  REQUIRE(room.meeting.until([&elsewhere] { return elsewhere.state() == NetworkSession::State::Online; }));
  CHECK(elsewhere.room() != room.opener.room());
  CHECK(room.meeting.relay.room_count() == 2);
  room.opener.send("ours", 1);
  REQUIRE(room.meeting.until(
      [&room] { return room.meeting.count(RoomOfThree::kThirdSeat, NetworkEvent::Kind::Message) == 1; }));
  CHECK(room.meeting.count(3, NetworkEvent::Kind::Message) == 0);
  CHECK(elsewhere.peers().empty());
}

TEST_CASE("a guest leaving a room is told to the others; the opener leaving ends it") {
  RoomOfThree room;
  room.third.leave();
  REQUIRE(room.meeting.until([&room] {
    return room.meeting.count(RoomOfThree::kOpenerSeat, NetworkEvent::Kind::PeerLeft) == 1 &&
           room.meeting.count(RoomOfThree::kSecondSeat, NetworkEvent::Kind::PeerLeft) == 1;
  }));
  CHECK(room.opener.peers() == std::vector<int>{kFirstGuestId});
  room.opener.leave();
  REQUIRE(room.meeting.until(
      [&room] { return room.meeting.count(RoomOfThree::kSecondSeat, NetworkEvent::Kind::Disconnected) == 1; }));
  CHECK(room.second.state() == NetworkSession::State::Offline);
  REQUIRE(room.meeting.until([&room] { return room.meeting.relay.room_count() == 0; }));
  CHECK(room.meeting.relay.connection_count() == 0);
}

TEST_CASE("a wrong code, a relay that is not there, and strangers get nowhere") {
  Meeting meeting;
  NetworkSession guest;
  meeting.seat(guest);
  std::string error;
  REQUIRE(guest.join_room(kLocalhost, meeting.relay.port(), "ZZZZ", error));
  REQUIRE(meeting.until([&meeting] { return meeting.count(0, NetworkEvent::Kind::Failed) == 1; }));
  CHECK(guest.state() == NetworkSession::State::Offline);
  CHECK(guest.room().empty());
  CHECK_FALSE(guest.join_room(kLocalhost, meeting.relay.port(), "", error));

  // A machine that connects and says nothing, or nonsense, is let go.
  Socket silent = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
  Socket rude = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
  REQUIRE(meeting.until([&meeting] { return meeting.relay.connection_count() == 2; }));
  std::vector<std::uint8_t> nonsense = wire_frame(R"({"k":"m","n":"hit"})");  // a message before entering a room
  REQUIRE(meeting.until([&] { return rude.write(nonsense) == IoStatus::Ok; }));
  REQUIRE(meeting.until([&meeting] { return meeting.relay.connection_count() == 1; }));
  meeting.relay.poll(kPastJoinTimeout);
  CHECK(meeting.relay.connection_count() == 0);
  CHECK(meeting.relay.room_count() == 0);
}

TEST_CASE("a guest cannot pass for someone else in a room") {
  RoomOfThree room;
  // What the session will not do, a raw connection can try: claim to be the host.
  std::string error;
  Socket liar = Socket::connect_to(kLocalhost, room.meeting.relay.port(), error);
  std::vector<std::uint8_t> hello = wire_frame(std::string(R"({"k":"h","r":")") + room.opener.room() + R"("})");
  std::vector<std::uint8_t> lie = wire_frame(R"({"k":"m","n":"order","f":1,"d":"obey"})");
  REQUIRE(room.meeting.until([&] { return liar.write(hello) == IoStatus::Ok; }));
  REQUIRE(room.meeting.until([&room] { return room.opener.peers().size() == 3; }));
  REQUIRE(room.meeting.until([&] { return liar.write(lie) == IoStatus::Ok; }));
  REQUIRE(room.meeting.until(
      [&room] { return room.meeting.count(RoomOfThree::kSecondSeat, NetworkEvent::Kind::Message) == 1; }));
  CHECK(room.meeting.last(RoomOfThree::kSecondSeat, NetworkEvent::Kind::Message)->peer == kFirstGuestId + 2);
}

TEST_CASE("games on the local network are found without typing an address") {
  LanDiscovery host, finder;
  REQUIRE(host.announce("Friday night", kGamePort, kTestDiscoveryPort));
  host.set_players(3);
  CHECK(finder.games().empty());
  REQUIRE(finder.search(kTestDiscoveryPort));
  bool found = false;
  for (int round = 0; round < kMaxRounds && !found; ++round) {
    host.update(kRoundSeconds);
    finder.update(kRoundSeconds);
    found = !finder.games().empty();
    std::this_thread::sleep_for(kRoundPause);
  }
  REQUIRE(found);
  for (int round = 0; round < kSettleRounds; ++round) {  // time for its second answer to arrive too
    host.update(kRoundSeconds);
    finder.update(kRoundSeconds);
    std::this_thread::sleep_for(kRoundPause);
  }
  REQUIRE(finder.games().size() == 1);  // once, though it answers as this machine and by its address
  const NearbyGame& game = finder.games().front();
  CHECK(game.name == "Friday night");
  CHECK(game.port == kGamePort);
  CHECK(game.players == 3);
  CHECK_FALSE(game.address.empty());

  // A game that stops answering is forgotten.
  host.stop_announcing();
  finder.update(kPastForgetting);
  CHECK(finder.games().empty());
  finder.stop_searching();
  CHECK_FALSE(finder.searching());
}

TEST_CASE("what is not an answer to a search is ignored") {
  redgem::test::LogCapture log;
  std::string error;
  Datagrams listener = Datagrams::open_on(kTestDiscoveryPort, error);
  REQUIRE(listener.valid());
  {  // a search from a finder that is gone by now, the first to arrive
    Datagrams gone = Datagrams::open_on(0, error);
    REQUIRE(gone.send_to(kLocalhost, kTestDiscoveryPort, kSearchQuestion));
  }
  LanDiscovery finder;
  REQUIRE(finder.search(kTestDiscoveryPort));
  bool found = false;
  for (int round = 0; round < kMaxRounds && !found; ++round) {
    finder.update(kRoundSeconds);
    found = !finder.games().empty();
    answer_with_nonsense(listener);
    std::this_thread::sleep_for(kRoundPause);
  }
  REQUIRE(found);
  REQUIRE(finder.games().size() == 1);
  CHECK(finder.games().front().name == "real");
  CHECK(finder.games().front().protocol == wire::kProtocolVersion);
  // The game of another version is not listed (it could not be joined): the log says why.
  CHECK(log.contains("\"newer\""));
  CHECK(log.contains(wire::protocol_mismatch(wire::kProtocolVersion + 1)));
}

TEST_CASE("guessing room codes gets an address kept out for a while") {
  RoomOfThree room;
  std::string error;
  for (int guess = 0; guess < kTooManyGuesses; ++guess) {
    NetworkSession guesser;
    room.meeting.seat(guesser);
    const std::size_t seat = room.meeting.sessions.size() - 1;
    REQUIRE(guesser.join_room(kLocalhost, room.meeting.relay.port(), "WRONG" + std::to_string(guess), error));
    REQUIRE(room.meeting.until([&] { return room.meeting.count(seat, NetworkEvent::Kind::Failed) == 1; }));
    room.meeting.sessions.pop_back();
    room.meeting.events.pop_back();
  }
  // Even the right code no longer works from there ...
  NetworkSession late;
  room.meeting.seat(late);
  REQUIRE(late.join_room(kLocalhost, room.meeting.relay.port(), room.opener.room(), error));
  REQUIRE(room.meeting.until([&room] { return room.meeting.count(3, NetworkEvent::Kind::Failed) == 1; }));
  // ... while those already in the room play on, and after a while the address is let back in.
  CHECK(room.opener.send("still here", 1));
  REQUIRE(room.meeting.until(
      [&room] { return room.meeting.count(RoomOfThree::kSecondSeat, NetworkEvent::Kind::Message) == 1; }));
  for (float waited = 0.0f; waited < kPastKeptOut; waited += kQuietStep) {
    room.meeting.relay.poll(kQuietStep);
    for (NetworkSession* session : room.meeting.sessions) session->poll(kQuietStep);
    std::this_thread::sleep_for(kRoundPause);
  }
  REQUIRE(late.join_room(kLocalhost, room.meeting.relay.port(), room.opener.room(), error));
  REQUIRE(room.meeting.until([&late] { return late.state() == NetworkSession::State::Online; }));
}

TEST_CASE("one address can only open so many rooms") {
  Meeting meeting;
  std::vector<std::unique_ptr<NetworkSession>> openers;
  std::string error;
  for (int i = 0; i < kRoomsPerAddress + 1; ++i) {
    openers.push_back(std::make_unique<NetworkSession>());
    meeting.seat(*openers.back());
    REQUIRE(openers.back()->host_room(kLocalhost, meeting.relay.port(), error));
    REQUIRE(meeting.until([&] { return openers.back()->state() != NetworkSession::State::Connecting; }));
  }
  CHECK(meeting.relay.room_count() == kRoomsPerAddress);
  CHECK(openers.back()->state() == NetworkSession::State::Offline);
  // Closing one makes room for another.
  openers.front()->leave();
  REQUIRE(meeting.until([&meeting] { return meeting.relay.room_count() == kRoomsPerAddress - 1; }));
  REQUIRE(openers.back()->host_room(kLocalhost, meeting.relay.port(), error));
  REQUIRE(meeting.until([&] { return openers.back()->state() == NetworkSession::State::Online; }));
}

TEST_CASE("a relay told how many machines and rooms one address may hold keeps to that") {
  constexpr RelayLimits kSmallLimits{3, 1};
  Meeting meeting(kSmallLimits);
  NetworkSession opener, second_opener, first_guest, second_guest, one_too_many;
  for (NetworkSession* session : {&opener, &second_opener, &first_guest, &second_guest, &one_too_many}) {
    meeting.seat(*session);
  }
  std::string error;
  const auto settled = [](NetworkSession& session) { return session.state() != NetworkSession::State::Connecting; };
  // One room per address: the second one asked for is refused.
  REQUIRE(opener.host_room(kLocalhost, meeting.relay.port(), error));
  REQUIRE(meeting.until([&] { return opener.state() == NetworkSession::State::Online; }));
  REQUIRE(second_opener.host_room(kLocalhost, meeting.relay.port(), error));
  REQUIRE(meeting.until([&] { return settled(second_opener); }));
  CHECK(second_opener.state() == NetworkSession::State::Offline);
  CHECK(meeting.relay.room_count() == 1);
  // Three machines per address: two guests join the opener, a third is hung up on.
  for (NetworkSession* guest : {&first_guest, &second_guest, &one_too_many}) {
    REQUIRE(guest->join_room(kLocalhost, meeting.relay.port(), opener.room(), error));
    REQUIRE(meeting.until([&] { return settled(*guest); }));
  }
  CHECK(first_guest.state() == NetworkSession::State::Online);
  CHECK(second_guest.state() == NetworkSession::State::Online);
  CHECK(one_too_many.state() == NetworkSession::State::Offline);
}

TEST_CASE("a room whose opener goes silent is closed, and its guests are told the game is over") {
  Meeting meeting;
  NetworkSession guest;
  meeting.seat(guest);
  std::string error;
  // An opener that is not a real session: it opens the room and then freezes.
  Socket frozen = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
  std::vector<std::uint8_t> hello = wire_frame(R"({"k":"h","r":"new"})");
  REQUIRE(meeting.until([&] { return frozen.write(hello) == IoStatus::Ok; }));
  REQUIRE(meeting.until([&meeting] { return meeting.relay.room_count() == 1; }));
  std::vector<std::uint8_t> arrived;
  REQUIRE(meeting.until([&] { return frozen.read(arrived, kWelcomeBytes) == IoStatus::Ok; }));
  FrameBuffer reader;
  reader.append(arrived.data(), arrived.size());
  const std::string code = redgem::test::message_in(*reader.next())["r"];
  REQUIRE(guest.join_room(kLocalhost, meeting.relay.port(), code, error));
  REQUIRE(meeting.until([&guest] { return guest.state() == NetworkSession::State::Online; }));

  for (float waited = 0.0f; waited < kSilenceTimeoutSeconds + 3.0f * kQuietStep; waited += kQuietStep) {
    meeting.relay.poll(kQuietStep);
    for (NetworkEvent& event : guest.poll(kQuietStep)) meeting.events[0].push_back(std::move(event));
    std::this_thread::sleep_for(kRoundPause);
  }
  REQUIRE(meeting.until([&meeting] { return meeting.count(0, NetworkEvent::Kind::Disconnected) == 1; }));
  CHECK(meeting.relay.room_count() == 0);
  CHECK(meeting.relay.connection_count() == 0);
}

namespace {

// A room's opener and as many guests as it has places for, all from this machine's one address.
struct Party {
  Meeting meeting;
  NetworkSession opener;
  std::vector<std::unique_ptr<NetworkSession>> guests;

  Party() {
    std::string error;
    meeting.seat(opener);
    REQUIRE_MESSAGE(opener.host_room(kLocalhost, meeting.relay.port(), error), error);
    REQUIRE(meeting.until([this] { return opener.state() == NetworkSession::State::Online; }));
    for (int i = 0; i < kPartyGuests; ++i) {
      guests.push_back(std::make_unique<NetworkSession>());
      meeting.seat(*guests.back());
    }
  }

  // Every guest asks for the room with `code` in the same moment; true once none is still trying.
  bool everyone_tries(const std::string& code) {
    std::string error;
    for (const auto& guest : guests)
      REQUIRE_MESSAGE(guest->join_room(kLocalhost, meeting.relay.port(), code, error), error);
    return meeting.until([this] { return in_state(NetworkSession::State::Connecting) == 0; });
  }

  int in_state(NetworkSession::State state) const {
    int total = 0;
    for (const auto& guest : guests) total += guest->state() == state ? 1 : 0;
    return total;
  }
};

}  // namespace

TEST_CASE("a whole party behind one address joins a room at once, and hears all its host says") {
  Party party;
  REQUIRE(party.everyone_tries(party.opener.room()));
  CHECK(party.in_state(NetworkSession::State::Online) == kPartyGuests);
  REQUIRE(party.meeting.until([&party] { return party.opener.peers().size() == kPartyGuests; }));
  // The host says a lot to everyone in one go: the relay takes it as one machine's talk.
  for (int i = 0; i < kBroadcasts; ++i) CHECK(party.opener.send("level", std::string(kBroadcastBytes, 'x')));
  REQUIRE(party.meeting.until([&party] {
    return party.meeting.count(kPartyGuests, NetworkEvent::Kind::Message) == kBroadcasts &&
           party.meeting.count(1, NetworkEvent::Kind::Message) == kBroadcasts;
  }));
  CHECK(party.in_state(NetworkSession::State::Online) == kPartyGuests);
  CHECK(party.opener.state() == NetworkSession::State::Online);
}

TEST_CASE("a whole party getting a room's code wrong, twice, is not kept out for guessing") {
  Party party;
  for (const char* misheard : {"WRONG1", "WRONG2"}) {
    REQUIRE(party.everyone_tries(misheard));
    CHECK(party.in_state(NetworkSession::State::Offline) == kPartyGuests);
  }
  REQUIRE(party.everyone_tries(party.opener.room()));
  CHECK(party.in_state(NetworkSession::State::Online) == kPartyGuests);
}

TEST_CASE("what a machine said before it hung up is passed on to its room, and then it is gone") {
  Meeting meeting;
  RawMachine opener(meeting), talker(meeting);
  const std::string code = opener.open_room();
  REQUIRE(talker.say(R"({"k":"h","r":")" + code + R"("})"));
  CHECK(opener.next_message()["k"] == "j");
  std::vector<std::uint8_t> last_words;
  const std::vector<std::uint8_t> tiny = wire_frame(R"({"k":"m","n":"x"})");
  for (int i = 0; i < kFloodMessages; ++i) last_words.insert(last_words.end(), tiny.begin(), tiny.end());
  while (!last_words.empty()) REQUIRE(talker.socket.write(last_words) != IoStatus::Closed);
  talker.socket.close();
  int passed_on = 0;
  nlohmann::json heard = opener.next_message();
  for (; heard["k"] == "m"; heard = opener.next_message()) ++passed_on;
  CHECK(passed_on == kFloodMessages);  // more than one poll handles: none was lost to the hanging up
  CHECK(heard["k"] == "l");
}

#ifndef _WIN32
namespace {

constexpr rlim_t kFewDescriptors = 64;

// How many of the callers a system had no descriptor for it keeps waiting: one, as Linux does, or
// none, as the Mac does (it hangs up on them).
std::size_t callers_kept_without_descriptors() { return redgem::platform::is_mac() ? 0 : 1; }

// While one of these lives the process can open nothing: no file, and no connection either.
struct NoDescriptorsLeft {
  rlimit before{};
  std::vector<int> taken;

  NoDescriptorsLeft() {
    REQUIRE(getrlimit(RLIMIT_NOFILE, &before) == 0);
    rlimit few = before;
    few.rlim_cur = std::min(before.rlim_cur, kFewDescriptors);
    REQUIRE(setrlimit(RLIMIT_NOFILE, &few) == 0);
    for (int descriptor = dup(STDIN_FILENO); descriptor >= 0; descriptor = dup(STDIN_FILENO)) {
      taken.push_back(descriptor);
    }
  }
  ~NoDescriptorsLeft() {
    for (const int descriptor : taken) ::close(descriptor);
    setrlimit(RLIMIT_NOFILE, &before);
  }
};

}  // namespace

TEST_CASE("a relay that cannot take a newcomer rests before it asks again, instead of never sleeping") {
  Meeting meeting;
  std::string error;
  Socket caller = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
  REQUIRE(meeting.relay.wait(kLongWaitSeconds));  // the caller is there to be taken
  {
    const NoDescriptorsLeft nothing_to_give;
    meeting.relay.poll(kRoundSeconds);
    CHECK(meeting.relay.connection_count() == 0);
    // The caller still waits, and would end every wait at once: the relay sleeps a moment anyway.
    const auto before = std::chrono::steady_clock::now();
    CHECK_FALSE(meeting.relay.wait(kLongWaitSeconds));
    const auto slept = std::chrono::steady_clock::now() - before;
    CHECK(slept >= std::chrono::duration<float>(kAcceptRestSeconds / 2.0f));
    CHECK(slept < kLongWait);
  }
  // The rest is not over yet, then it is: the caller is taken, where the system kept it waiting,
  // and so is whoever calls next.
  meeting.relay.poll(kRoundSeconds);
  CHECK(meeting.relay.connection_count() == 0);
  meeting.relay.poll(kAcceptRestSeconds);
  const std::size_t kept = callers_kept_without_descriptors();
  CHECK(meeting.relay.connection_count() == kept);
  const Socket next_caller = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
  CHECK(meeting.until([&] { return meeting.relay.connection_count() == kept + 1; }));
}
#endif

TEST_CASE("a relay turns away a machine of another version of the protocol, saying why") {
  redgem::test::LogCapture log;
  Meeting meeting;
  RawMachine newer(meeting);
  const int next_protocol = wire::kProtocolVersion + 1;
  REQUIRE(newer.say(R"({"k":"h","r":"new","v":)" + std::to_string(next_protocol) + "}"));
  const nlohmann::json refusal = newer.next_message();
  CHECK(refusal[wire::kKind] == wire::kRefused);
  CHECK(refusal[wire::kReason] == wire::protocol_mismatch(next_protocol));
  CHECK(refusal[wire::kVersion] == wire::kProtocolVersion);
  CHECK(meeting.relay.room_count() == 0);
  CHECK(log.contains(wire::protocol_mismatch(next_protocol)));
  // Nor is one whose hello this version could not even read.
  RawMachine odd(meeting);
  REQUIRE(odd.say(R"({"k":"h","r":["new"],"v":)" + std::to_string(next_protocol) + "}"));
  CHECK(odd.next_message()[wire::kReason] == wire::protocol_mismatch(next_protocol));
  // A session of this version gets its room.
  NetworkSession opener;
  meeting.seat(opener);
  std::string error;
  REQUIRE(opener.host_room(kLocalhost, meeting.relay.port(), error));
  REQUIRE(meeting.until([&opener] { return opener.state() == NetworkSession::State::Online; }));
}
