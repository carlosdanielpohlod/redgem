#include <doctest/doctest.h>

#include <chrono>
#include <functional>
#include <set>
#include <thread>

#include "net/lan_discovery.hpp"
#include "net/network_session.hpp"
#include "net/relay_server.hpp"

using namespace redgem::net;

namespace {

constexpr int kMaxRounds = 3000;  // of polling, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr float kRoundSeconds = 0.001f;
constexpr float kPastJoinTimeout = 6.0f;
constexpr float kPastForgetting = 4.0f;  // seconds after which a silent game is forgotten
constexpr int kSettleRounds = 50;
constexpr int kTooManyGuesses = 12;  // more wrong room codes than one address is allowed
constexpr int kRoomsPerAddress = 8;
constexpr float kPastKeptOut = 125.0f;  // seconds an address stays out for guessing
constexpr float kQuietStep = 4.0f;
constexpr float kAskAgain = 1.1f;  // seconds: past the pause between searches
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

  explicit Meeting(std::size_t buffered_bytes = kMaxRelayBufferedBytes) : relay(buffered_bytes) {
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

  explicit RawMachine(Meeting& where) : meeting(where) {
    std::string error;
    socket = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
    REQUIRE(socket.valid());
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

  bool say(const std::string& text) { return send(frame(text)); }

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
    return nlohmann::json::parse(*text);
  }

  // Opens a room and returns its code.
  std::string open_room() {
    REQUIRE(say(R"({"k":"h","r":"new"})"));
    return next_message()["r"];
  }
};

}  // namespace

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
  const std::vector<std::uint8_t> tiny = frame(R"({"k":"m","n":"x"})");
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
  std::vector<std::uint8_t> nonsense = frame(R"({"k":"m","n":"hit"})");  // a message before entering a room
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
  std::vector<std::uint8_t> hello = frame(std::string(R"({"k":"h","r":")") + room.opener.room() + R"("})");
  std::vector<std::uint8_t> lie = frame(R"({"k":"m","n":"order","f":1,"d":"obey"})");
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
  LanDiscovery finder;
  REQUIRE(finder.search(kTestDiscoveryPort));
  std::string error;
  Datagrams listener = Datagrams::open_on(kTestDiscoveryPort, error);
  REQUIRE(listener.valid());
  // Whoever answers the search with nonsense, or with half an answer.
  std::string question, from;
  std::uint16_t from_port = 0;
  bool asked = false;
  for (int round = 0; round < kMaxRounds && !asked; ++round) {
    finder.update(kAskAgain);
    asked = listener.receive(question, from, from_port);
    std::this_thread::sleep_for(kRoundPause);
  }
  REQUIRE(asked);
  for (const char* bad : {"hello", R"({"is":"redgem"})", R"({"is":"redgem","name":7,"port":7777,"players":1})",
                          R"({"is":"redgem","name":"x","port":99999,"players":1})",
                          R"({"is":"other","name":"x","port":7777,"players":1})"}) {
    listener.send_to(kLocalhost, from_port, bad);
  }
  listener.send_to(kLocalhost, from_port, R"({"is":"redgem","name":"real","port":7777,"players":2})");
  bool found = false;
  for (int round = 0; round < kMaxRounds && !found; ++round) {
    finder.update(kRoundSeconds);
    found = !finder.games().empty();
    std::this_thread::sleep_for(kRoundPause);
  }
  REQUIRE(found);
  REQUIRE(finder.games().size() == 1);
  CHECK(finder.games().front().name == "real");
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

TEST_CASE("a room whose opener goes silent is closed, and its guests are told the game is over") {
  Meeting meeting;
  NetworkSession guest;
  meeting.seat(guest);
  std::string error;
  // An opener that is not a real session: it opens the room and then freezes.
  Socket frozen = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
  std::vector<std::uint8_t> hello = frame(R"({"k":"h","r":"new"})");
  REQUIRE(meeting.until([&] { return frozen.write(hello) == IoStatus::Ok; }));
  REQUIRE(meeting.until([&meeting] { return meeting.relay.room_count() == 1; }));
  std::vector<std::uint8_t> arrived;
  REQUIRE(meeting.until([&] { return frozen.read(arrived, kWelcomeBytes) == IoStatus::Ok; }));
  FrameBuffer reader;
  reader.append(arrived.data(), arrived.size());
  const std::string code = nlohmann::json::parse(*reader.next())["r"];
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
