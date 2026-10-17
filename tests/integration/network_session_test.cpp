#include <doctest/doctest.h>

#include <chrono>
#include <functional>
#include <thread>

#include "integration/wire_text.hpp"
#include "net/network_session.hpp"
#include "test_support.hpp"

using namespace redgem::net;
using redgem::test::wire_frame;

namespace {

constexpr int kMaxRounds = 3000;  // of polling, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr float kRoundSeconds = 0.001f;
constexpr float kPastJoinTimeout = 6.0f;  // seconds: longer than a join waits
constexpr const char* kLocalhost = "127.0.0.1";
constexpr int kFirstGuest = kHostId + 1;
constexpr int kSecondGuest = kHostId + 2;
constexpr std::size_t kBigMessageBytes = 300 * 1024;  // more than one read or write takes
constexpr int kTooDeep = kMaxDataDepth + 4;           // arrays inside arrays
constexpr std::size_t kTooLongName = kMaxNameBytes + 1;
constexpr int kSettleRounds = 50;
constexpr int kManyVisits = 300;    // more than the largest id there is
constexpr float kQuietStep = 4.0f;  // seconds
constexpr float kPastSilenceTimeout = kSilenceTimeoutSeconds + 3.0f * kQuietStep;
constexpr std::size_t kShownBytes = 60;          // of a bad message, when a check fails
constexpr int kSilentStrangers = kMaxPeers - 1;  // as many as a game has places for guests
constexpr int kFloodMessages = 3 * kMaxMessagesPerPoll / 2;
constexpr std::chrono::milliseconds kArrivalPause{50};     // for what was written to be there to read
constexpr int kNextProtocol = wire::kProtocolVersion + 1;  // what a newer version of the engine speaks

// Sessions on this machine, and everything that happened to each.
struct Table {
  std::vector<NetworkSession*> sessions;
  std::vector<std::vector<NetworkEvent>> events;

  void seat(NetworkSession& session) {
    sessions.push_back(&session);
    events.emplace_back();
  }

  // Polls everyone until `done` (false if it never is).
  bool until(const std::function<bool()>& done) {
    for (int round = 0; round < kMaxRounds; ++round) {
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

// A host and two guests, all in the game.
struct Party {
  NetworkSession host, first, second;
  Table table;
  static constexpr std::size_t kHostSeat = 0, kFirstSeat = 1, kSecondSeat = 2;

  Party() {
    std::string error;
    REQUIRE_MESSAGE(host.host(0, error), error);
    table.seat(host);
    table.seat(first);
    table.seat(second);
    REQUIRE_MESSAGE(first.join(kLocalhost, host.port(), error), error);
    REQUIRE(table.until([this] { return first.state() == NetworkSession::State::Online; }));
    REQUIRE_MESSAGE(second.join(kLocalhost, host.port(), error), error);
    REQUIRE(table.until([this] {
      return second.state() == NetworkSession::State::Online && first.peers().size() == 2 && host.peers().size() == 2;
    }));
  }
};

}  // namespace

TEST_CASE("a host and the machines that join it know who is in the game") {
  Party party;
  CHECK(party.host.hosting());
  CHECK(party.host.id() == kHostId);
  CHECK(party.first.id() == kFirstGuest);
  CHECK(party.second.id() == kSecondGuest);
  CHECK_FALSE(party.first.hosting());
  CHECK(party.host.peers() == std::vector<int>{kFirstGuest, kSecondGuest});
  CHECK(party.first.peers() == std::vector<int>{kHostId, kSecondGuest});
  CHECK(party.second.peers() == std::vector<int>{kHostId, kFirstGuest});
  // Everyone was told once that they are in, and about each arrival after their own.
  for (const std::size_t seat : {Party::kHostSeat, Party::kFirstSeat, Party::kSecondSeat}) {
    CHECK(party.table.count(seat, NetworkEvent::Kind::Connected) == 1);
  }
  CHECK(party.table.count(Party::kHostSeat, NetworkEvent::Kind::PeerJoined) == 2);
  CHECK(party.table.count(Party::kFirstSeat, NetworkEvent::Kind::PeerJoined) == 1);
  CHECK(party.table.count(Party::kSecondSeat, NetworkEvent::Kind::PeerJoined) == 0);
}

TEST_CASE("a message to everyone reaches everyone else, with who sent it and what it carries") {
  Party party;
  CHECK(party.first.send("hit", {{"damage", 3}, {"at", {1.5, 2.0, -4.0}}}));
  REQUIRE(party.table.until([&party] {
    return party.table.count(Party::kHostSeat, NetworkEvent::Kind::Message) == 1 &&
           party.table.count(Party::kSecondSeat, NetworkEvent::Kind::Message) == 1;
  }));
  for (const std::size_t seat : {Party::kHostSeat, Party::kSecondSeat}) {
    const NetworkEvent* message = party.table.last(seat, NetworkEvent::Kind::Message);
    CHECK(message->peer == kFirstGuest);
    CHECK(message->name == "hit");
    CHECK(message->data["damage"] == 3);
    CHECK(message->data["at"][2] == -4.0);
  }
  CHECK(party.table.count(Party::kFirstSeat, NetworkEvent::Kind::Message) == 0);  // not back to the sender
}

TEST_CASE("a message to one peer reaches only that peer, through the host when needed") {
  Party party;
  CHECK(party.first.send("whisper", "psst", kSecondGuest));
  CHECK(party.host.send("order", 7, kFirstGuest));
  CHECK(party.second.send("report", true, kHostId));
  REQUIRE(party.table.until([&party] {
    return party.table.count(Party::kHostSeat, NetworkEvent::Kind::Message) == 1 &&
           party.table.count(Party::kFirstSeat, NetworkEvent::Kind::Message) == 1 &&
           party.table.count(Party::kSecondSeat, NetworkEvent::Kind::Message) == 1;
  }));
  CHECK(party.table.last(Party::kSecondSeat, NetworkEvent::Kind::Message)->name == "whisper");
  CHECK(party.table.last(Party::kSecondSeat, NetworkEvent::Kind::Message)->peer == kFirstGuest);
  CHECK(party.table.last(Party::kFirstSeat, NetworkEvent::Kind::Message)->data == 7);
  CHECK(party.table.last(Party::kHostSeat, NetworkEvent::Kind::Message)->peer == kSecondGuest);
  // Nobody to send to: oneself, or someone who is not in the game.
  CHECK_FALSE(party.first.send("echo", 1, kFirstGuest));
  CHECK_FALSE(party.first.send("lost", 1, kSecondGuest + 5));
  CHECK_FALSE(party.host.send("lost", 1, kSecondGuest + 5));
}

TEST_CASE("messages arrive in the order they were sent, however long they are") {
  Party party;
  const std::string big(kBigMessageBytes, 'x');
  CHECK(party.host.send("first", big));
  CHECK(party.host.send("second", 2));
  REQUIRE(
      party.table.until([&party] { return party.table.count(Party::kFirstSeat, NetworkEvent::Kind::Message) == 2; }));
  std::vector<std::string> names;
  for (const NetworkEvent& event : party.table.events[Party::kFirstSeat]) {
    if (event.kind == NetworkEvent::Kind::Message) names.push_back(event.name);
    if (event.name == "first") CHECK(event.data.get<std::string>().size() == kBigMessageBytes);
  }
  CHECK(names == std::vector<std::string>{"first", "second"});
}

TEST_CASE("when a guest leaves, the others are told; when the host leaves, the game is over") {
  Party party;
  party.second.leave();
  CHECK(party.second.state() == NetworkSession::State::Offline);
  REQUIRE(party.table.until([&party] {
    return party.table.count(Party::kHostSeat, NetworkEvent::Kind::PeerLeft) == 1 &&
           party.table.count(Party::kFirstSeat, NetworkEvent::Kind::PeerLeft) == 1;
  }));
  CHECK(party.table.last(Party::kFirstSeat, NetworkEvent::Kind::PeerLeft)->peer == kSecondGuest);
  CHECK(party.host.peers() == std::vector<int>{kFirstGuest});
  CHECK(party.first.peers() == std::vector<int>{kHostId});

  party.host.leave();
  REQUIRE(party.table.until(
      [&party] { return party.table.count(Party::kFirstSeat, NetworkEvent::Kind::Disconnected) == 1; }));
  CHECK(party.first.state() == NetworkSession::State::Offline);
  CHECK(party.first.id() == 0);
  CHECK_FALSE(party.first.send("anyone", 1));
}

TEST_CASE("joining a game nobody hosts fails, and so does hosting on a port in use") {
  std::string error;
  NetworkSession host;
  REQUIRE_MESSAGE(host.host(0, error), error);
  const std::uint16_t port = host.port();
  NetworkSession second_host;
  CHECK_FALSE(second_host.host(port, error));
  CHECK_FALSE(error.empty());
  host.leave();

  NetworkSession guest;
  Table table;
  table.seat(guest);
  if (guest.join(kLocalhost, port, error)) {  // nobody listens there any more
    CHECK(guest.state() == NetworkSession::State::Connecting);
    REQUIRE(table.until([&table] { return table.count(0, NetworkEvent::Kind::Failed) == 1; }));
  }
  CHECK(guest.state() == NetworkSession::State::Offline);
  CHECK_FALSE(guest.join("no such machine.invalid", port, error));
}

TEST_CASE("a join that is never answered gives up") {
  // Something that accepts connections and then says nothing: a listening socket nobody polls.
  std::string error;
  Socket silent = Socket::listen_on(0, error);
  REQUIRE(silent.valid());
  NetworkSession guest;
  REQUIRE(guest.join(kLocalhost, silent.local_port(), error));
  CHECK(guest.poll(kRoundSeconds).empty());
  const std::vector<NetworkEvent> events = guest.poll(kPastJoinTimeout);
  REQUIRE(events.size() == 1);
  CHECK(events.front().kind == NetworkEvent::Kind::Failed);
  CHECK(guest.state() == NetworkSession::State::Offline);
}

namespace {

// Something that is not this engine, talking to a host over a plain connection.
struct Stranger {
  Socket socket;
  Table& table;

  Stranger(NetworkSession& host, Table& host_table) : table(host_table) {
    std::string error;
    socket = Socket::connect_to(kLocalhost, host.port(), error);
    REQUIRE(socket.valid());
    REQUIRE(table.until([this] { return socket.connect_status() == IoStatus::Ok; }));
  }

  void say(const std::string& text) { say_bytes(wire_frame(text)); }

  void say_bytes(std::vector<std::uint8_t> bytes) {
    REQUIRE(table.until([&] { return socket.write(bytes) == IoStatus::Ok; }));
  }

  // The next message the host sent it, waited for (null when none came).
  nlohmann::json hear() {
    FrameBuffer heard;
    std::optional<std::string> bytes;
    table.until([&] {
      std::vector<std::uint8_t> arrived;
      socket.read(arrived, kBigMessageBytes);
      heard.append(arrived.data(), arrived.size());
      bytes = heard.next();
      return bytes.has_value();
    });
    return bytes ? redgem::test::message_in(*bytes) : nlohmann::json();
  }

  // Says hello as a real guest would, and waits to be let in.
  void get_in(NetworkSession& host) {
    say(R"({"k":"h"})");
    REQUIRE(table.until([&host] { return host.peers().size() == 1; }));
  }

  bool hung_up_on() {
    return table.until([this] {
      std::vector<std::uint8_t> ignored;
      return socket.read(ignored, kBigMessageBytes) == IoStatus::Closed;
    });
  }
};

struct LoneHost {
  NetworkSession host;
  Table table;

  LoneHost() {
    std::string error;
    REQUIRE(host.host(0, error));
    table.seat(host);
  }
};

}  // namespace

TEST_CASE("whoever connects without saying hello is never in the game") {
  LoneHost lone;
  Stranger talker(lone.host, lone.table);
  talker.say_bytes(redgem::test::raw_frame("this is not a message"));
  CHECK(talker.hung_up_on());
  Stranger rude(lone.host, lone.table);
  rude.say(R"({"k":"m","n":"hit","d":1})");  // a message before the hello
  CHECK(rude.hung_up_on());
  // And one that connects and says nothing is dropped after a while.
  Stranger silent(lone.host, lone.table);
  REQUIRE(lone.table.until([] { return true; }));
  lone.host.poll(kPastJoinTimeout);
  CHECK(silent.hung_up_on());
  CHECK(lone.host.peers().empty());
  CHECK(lone.table.count(0, NetworkEvent::Kind::PeerJoined) == 0);
  CHECK(lone.table.count(0, NetworkEvent::Kind::PeerLeft) == 0);
}

TEST_CASE("a guest sending messages of the wrong shape is dropped, and the host carries on") {
  const std::string deep = R"({"k":"m","n":"x","d":)" + std::string(kTooDeep, '[') + std::string(kTooDeep, ']') + "}";
  const std::string long_name = R"({"k":"m","n":")" + std::string(kTooLongName, 'n') + R"("})";
  const std::string long_key = R"({"k":"m","n":"x","d":{")" + std::string(kTooLongName, 'k') + R"(":1}})";
  for (const std::string& bad : {std::string(R"({"k":"m","n":"x","t":"everyone"})"), std::string(R"({"k":"m","n":7})"),
                                 std::string(R"({"k":"m","n":"x","f":-4})"), std::string(R"({"k":7})"),
                                 std::string(R"([1,2,3])"), deep, long_name, long_key}) {
    CAPTURE(bad.substr(0, kShownBytes));
    LoneHost lone;
    Stranger guest(lone.host, lone.table);
    guest.get_in(lone.host);
    guest.say(bad);
    REQUIRE(lone.table.until([&lone] { return lone.table.count(0, NetworkEvent::Kind::PeerLeft) == 1; }));
    CHECK(lone.host.peers().empty());
    CHECK(lone.table.count(0, NetworkEvent::Kind::Message) == 0);
    CHECK(lone.host.hosting());
  }
}

TEST_CASE("a guest is not fooled by a host that hands out the host's own id") {
  std::string error;
  Socket fake_host = Socket::listen_on(0, error);
  REQUIRE(fake_host.valid());
  NetworkSession guest;
  Table table;
  table.seat(guest);
  REQUIRE(guest.join(kLocalhost, fake_host.local_port(), error));
  Socket link;
  REQUIRE(table.until([&] {
    if (!link.valid()) fake_host.accept(link);
    return link.valid();
  }));
  std::vector<std::uint8_t> welcome = wire_frame(R"({"k":"w","t":1,"p":[]})");
  REQUIRE(table.until([&] { return link.write(welcome) == IoStatus::Ok; }));
  REQUIRE(table.until([&table] { return table.count(0, NetworkEvent::Kind::Failed) == 1; }));
  CHECK(guest.state() == NetworkSession::State::Offline);
}

TEST_CASE("what cannot be received is not sent") {
  Party party;
  CHECK_FALSE(party.host.send(std::string(kTooLongName, 'n'), 1));
  nlohmann::json deep = 1;
  for (int i = 0; i < kTooDeep; ++i) deep = nlohmann::json::array({deep});
  CHECK_FALSE(party.host.send("deep", deep));
  CHECK(party.host.send("text", std::string("broken \xff utf-8")));  // sent with the bad byte replaced
  REQUIRE(
      party.table.until([&party] { return party.table.count(Party::kFirstSeat, NetworkEvent::Kind::Message) == 1; }));
  CHECK(party.table.last(Party::kFirstSeat, NetworkEvent::Kind::Message)->name == "text");
}

TEST_CASE("ids are handed out again once free, however many machines come and go") {
  LoneHost lone;
  NetworkSession regular;
  lone.table.seat(regular);
  std::string error;
  REQUIRE(regular.join(kLocalhost, lone.host.port(), error));
  REQUIRE(lone.table.until([&regular] { return regular.state() == NetworkSession::State::Online; }));
  // More comings and goings than there are numbers a message may carry as an id.
  for (int visit = 0; visit < kManyVisits; ++visit) {
    Stranger visitor(lone.host, lone.table);
    visitor.say(R"({"k":"h"})");
    REQUIRE(lone.table.until([&lone] { return lone.host.peers().size() == 2; }));
    CHECK(lone.host.peers().back() == kSecondGuest);  // always the lowest id that is free
    visitor.socket.close();
    REQUIRE(lone.table.until([&lone] { return lone.host.peers().size() == 1; }));
  }
  // The machine that stayed heard every arrival and is still in the game.
  REQUIRE(lone.table.until([&] { return lone.table.count(1, NetworkEvent::Kind::PeerLeft) == kManyVisits; }));
  CHECK(regular.state() == NetworkSession::State::Online);
  CHECK(lone.table.count(1, NetworkEvent::Kind::Disconnected) == 0);
}

TEST_CASE("a machine that goes silent without hanging up is given up on; one that only has nothing to say is not") {
  LoneHost lone;
  NetworkSession quiet_guest;
  lone.table.seat(quiet_guest);
  std::string error;
  REQUIRE(quiet_guest.join(kLocalhost, lone.host.port(), error));
  REQUIRE(lone.table.until([&lone] { return lone.host.peers().size() == 1; }));
  Stranger frozen(lone.host, lone.table);
  frozen.say(R"({"k":"h"})");
  REQUIRE(lone.table.until([&lone] { return lone.host.peers().size() == 2; }));

  // A long while passes, in steps shorter than the time between "I am here"s.
  for (float waited = 0.0f; waited < kPastSilenceTimeout; waited += kQuietStep) {
    lone.host.poll(kQuietStep);
    for (NetworkEvent& event : quiet_guest.poll(kQuietStep)) lone.table.events[1].push_back(std::move(event));
    std::this_thread::sleep_for(kRoundPause);
  }
  REQUIRE(lone.table.until([&lone] { return lone.host.peers().size() == 1; }));
  CHECK(lone.host.peers() == std::vector<int>{kFirstGuest});  // the real guest, which said nothing either
  CHECK(quiet_guest.state() == NetworkSession::State::Online);
}

TEST_CASE("only a few machines from one address may keep a host waiting for their hello") {
  LoneHost lone;
  std::vector<std::unique_ptr<Stranger>> silent;
  for (int i = 0; i < kSilentStrangers; ++i) silent.push_back(std::make_unique<Stranger>(lone.host, lone.table));
  // Only a few from one address are kept waiting for their hello; the rest are hung up on at once.
  CHECK(silent.back()->hung_up_on());
  std::vector<std::uint8_t> ignored;
  CHECK(silent.front()->socket.read(ignored, kBigMessageBytes) == IoStatus::WouldBlock);
  // Once the quiet ones are given up on, a guest from the same address gets in: no place was taken.
  lone.host.poll(kPastJoinTimeout);
  NetworkSession guest;
  lone.table.seat(guest);
  std::string error;
  REQUIRE(guest.join(kLocalhost, lone.host.port(), error));
  REQUIRE(lone.table.until([&guest] { return guest.state() != NetworkSession::State::Connecting; }));
  CHECK(guest.state() == NetworkSession::State::Online);
  CHECK(lone.host.peers() == std::vector<int>{kFirstGuest});
}

TEST_CASE("a host told to let more machines from one address wait for their hello lets that many wait") {
  constexpr int kWaiting = 2 * kDefaultWaitingPerAddress;
  NetworkSession host;
  Table table;
  std::string error;
  REQUIRE(host.host(0, error, kWaiting));
  table.seat(host);
  std::vector<std::unique_ptr<Stranger>> silent;
  for (int i = 0; i < kWaiting + 1; ++i) silent.push_back(std::make_unique<Stranger>(host, table));
  CHECK(silent.back()->hung_up_on());
  std::vector<std::uint8_t> ignored;
  CHECK(silent[kWaiting - 1]->socket.read(ignored, kBigMessageBytes) == IoStatus::WouldBlock);
}

TEST_CASE("a flood of tiny messages from one machine is handled a poll's worth at a time") {
  LoneHost lone;
  Stranger flooder(lone.host, lone.table);
  flooder.get_in(lone.host);
  std::vector<std::uint8_t> flood;
  const std::vector<std::uint8_t> tiny = wire_frame(R"({"k":"m","n":"x"})");
  for (int i = 0; i < kFloodMessages; ++i) flood.insert(flood.end(), tiny.begin(), tiny.end());
  REQUIRE(lone.table.until([&] { return flooder.socket.write(flood) == IoStatus::Ok && flood.empty(); }));
  std::this_thread::sleep_for(kArrivalPause);
  int most_in_a_poll = 0;
  int heard = 0;
  for (int round = 0; round < kMaxRounds && heard < kFloodMessages; ++round) {
    const int in_this_poll = static_cast<int>(lone.host.poll(kRoundSeconds).size());
    most_in_a_poll = std::max(most_in_a_poll, in_this_poll);
    heard += in_this_poll;
  }
  CHECK(heard == kFloodMessages);  // none is lost: the rest waited for the next poll
  CHECK(most_in_a_poll <= kMaxMessagesPerPoll);
}

TEST_CASE("what a guest said before it hung up is all heard, and only then is it gone") {
  LoneHost lone;
  Stranger talker(lone.host, lone.table);
  talker.get_in(lone.host);
  std::vector<std::uint8_t> last_words;
  const std::vector<std::uint8_t> tiny = wire_frame(R"({"k":"m","n":"x"})");
  for (int i = 0; i < kFloodMessages; ++i) last_words.insert(last_words.end(), tiny.begin(), tiny.end());
  while (!last_words.empty()) REQUIRE(talker.socket.write(last_words) != IoStatus::Closed);
  talker.socket.close();
  REQUIRE(lone.table.until([&lone] { return lone.table.count(0, NetworkEvent::Kind::PeerLeft) == 1; }));
  CHECK(lone.table.count(0, NetworkEvent::Kind::Message) == kFloodMessages);  // more than one poll handles
  CHECK(lone.table.events[0].back().kind == NetworkEvent::Kind::PeerLeft);
}

namespace {

// Something that takes a guest's connection and answers as it pleases, as a hostile host would.
struct FakeHost {
  Socket listener, link;
  NetworkSession guest;
  Table table;

  FakeHost() {
    std::string error;
    listener = Socket::listen_on(0, error);
    REQUIRE(listener.valid());
    table.seat(guest);
    REQUIRE(guest.join(kLocalhost, listener.local_port(), error));
    REQUIRE(table.until([this] {
      if (!link.valid()) listener.accept(link);
      return link.valid();
    }));
  }

  void say(const std::string& text) {
    std::vector<std::uint8_t> bytes = wire_frame(text);
    REQUIRE(table.until([&] { return link.write(bytes) == IoStatus::Ok; }));
  }
};

}  // namespace

TEST_CASE("a room's opener hears of each newcomer asking to come in once, however often the relay says it") {
  std::string error;
  Socket listener = Socket::listen_on(0, error);
  REQUIRE(listener.valid());
  Socket relay;
  NetworkSession opener;
  Table table;
  table.seat(opener);
  opener.require_approval(true);
  REQUIRE(opener.host_room(kLocalhost, listener.local_port(), error));
  REQUIRE(table.until([&] { return listener.accept(relay) == IoStatus::Ok || relay.valid(); }));
  const auto say = [&](const std::string& text) {
    std::vector<std::uint8_t> bytes = wire_frame(text);
    REQUIRE(table.until([&] { return relay.write(bytes) == IoStatus::Ok && bytes.empty(); }));
  };
  say(R"({"k":"w","t":1,"p":[],"r":"ABCDEF"})");
  REQUIRE(table.until([&] { return opener.state() == NetworkSession::State::Online; }));
  for (int again = 0; again < kManyVisits; ++again) say(R"({"k":"q","f":2})");
  say(R"({"k":"q","f":3})");
  REQUIRE(table.until([&] { return table.count(0, NetworkEvent::Kind::Approval) == 2; }));
  int rounds = 0;
  table.until([&rounds] { return ++rounds == kSettleRounds; });  // for any more to arrive
  CHECK(table.count(0, NetworkEvent::Kind::Approval) == 2);
  CHECK(opener.deciding() == 2);
}

TEST_CASE("a guest does not believe a welcome whose list of the others cannot be true") {
  std::string crowd = "[1";
  for (int id = kFirstGuest + 1; id <= kMaxPeers + 1; ++id) crowd += "," + std::to_string(id);
  crowd += "]";
  for (const std::string& others : {std::string("[1,2]"), std::string("[1,3,3]"), std::string("[1,0]"),
                                    std::string("[1,99]"), std::string("[1,-3]"), crowd}) {
    CAPTURE(others);
    FakeHost fake;
    fake.say(R"({"k":"w","t":2,"p":)" + others + "}");
    REQUIRE(fake.table.until([&fake] { return fake.table.count(0, NetworkEvent::Kind::Failed) == 1; }));
    CHECK(fake.guest.state() == NetworkSession::State::Offline);
    CHECK(fake.guest.peers().empty());
  }
}

TEST_CASE("a guest hears of each arrival once, and of no departure of someone it never heard of") {
  FakeHost fake;
  fake.say(R"({"k":"w","t":2,"p":[1]})");
  REQUIRE(fake.table.until([&fake] { return fake.guest.state() == NetworkSession::State::Online; }));
  for (const char* claim : {R"({"k":"j","f":3})", R"({"k":"j","f":3})", R"({"k":"j","f":2})", R"({"k":"j","f":1})",
                            R"({"k":"j","f":0})", R"({"k":"j"})", R"({"k":"l","f":9})", R"({"k":"l","f":2})"}) {
    fake.say(claim);
  }
  fake.say(R"({"k":"m","n":"done","f":1})");
  REQUIRE(fake.table.until([&fake] { return fake.table.count(0, NetworkEvent::Kind::Message) == 1; }));
  CHECK(fake.guest.peers() == std::vector<int>{kHostId, kSecondGuest});
  CHECK(fake.table.count(0, NetworkEvent::Kind::PeerJoined) == 1);
  CHECK(fake.table.count(0, NetworkEvent::Kind::PeerLeft) == 0);
  // Someone it does know of still leaves.
  fake.say(R"({"k":"l","f":3})");
  REQUIRE(fake.table.until([&fake] { return fake.table.count(0, NetworkEvent::Kind::PeerLeft) == 1; }));
  CHECK(fake.guest.peers() == std::vector<int>{kHostId});
}

TEST_CASE("what a host said before it hung up is all heard, and only then is the game over") {
  FakeHost fake;
  fake.say(R"({"k":"w","t":2,"p":[1]})");
  std::vector<std::uint8_t> last_words;
  const std::vector<std::uint8_t> tiny = wire_frame(R"({"k":"m","n":"x","f":1})");
  for (int i = 0; i < kFloodMessages; ++i) last_words.insert(last_words.end(), tiny.begin(), tiny.end());
  while (!last_words.empty()) REQUIRE(fake.link.write(last_words) != IoStatus::Closed);
  fake.link.close();
  REQUIRE(fake.table.until([&fake] { return fake.table.count(0, NetworkEvent::Kind::Disconnected) == 1; }));
  CHECK(fake.table.count(0, NetworkEvent::Kind::Message) == kFloodMessages);
  CHECK(fake.table.events[0].back().kind == NetworkEvent::Kind::Disconnected);
}

TEST_CASE("a newcomer of another version of the protocol is turned away, told why, and the host logs it") {
  redgem::test::LogCapture log;
  LoneHost lone;
  Stranger newer(lone.host, lone.table);
  newer.say(R"({"k":"h","v":)" + std::to_string(kNextProtocol) + "}");
  const nlohmann::json refusal = newer.hear();
  CHECK(refusal[wire::kKind] == wire::kRefused);
  CHECK(refusal[wire::kVersion] == wire::kProtocolVersion);
  CHECK(refusal[wire::kReason] == wire::protocol_mismatch(kNextProtocol));
  CHECK(newer.hung_up_on());
  CHECK(lone.host.peers().empty());
  CHECK(log.contains("this game runs protocol " + std::to_string(wire::kProtocolVersion) + ", the other side " +
                     std::to_string(kNextProtocol)));
  // A hello that says no version at all is of none this host speaks either.
  Stranger unversioned(lone.host, lone.table);
  unversioned.say(R"({"k":"h","v":0})");
  CHECK(unversioned.hear()[wire::kReason] == wire::protocol_mismatch(0));
}

TEST_CASE("a guest turned away by a host of another version says why in its own words") {
  for (const std::string& answer : {R"({"k":"x","y":"whatever its reason","v":)" + std::to_string(kNextProtocol) + "}",
                                    R"({"k":"w","t":2,"p":[1],"v":)" + std::to_string(kNextProtocol) + "}",
                                    // (a welcome this version could not read: its list of the others is no list)
                                    R"({"k":"w","t":2,"p":"all","v":)" + std::to_string(kNextProtocol) + "}"}) {
    CAPTURE(answer);
    redgem::test::LogCapture log;
    FakeHost fake;
    fake.say(answer);
    REQUIRE(fake.table.until([&fake] { return fake.table.count(0, NetworkEvent::Kind::Failed) == 1; }));
    CHECK(fake.table.last(0, NetworkEvent::Kind::Failed)->name == wire::protocol_mismatch(kNextProtocol));
    CHECK(fake.guest.state() == NetworkSession::State::Offline);
    CHECK(log.contains(wire::protocol_mismatch(kNextProtocol)));
  }
}

TEST_CASE("a refusal from a host of this version keeps the host's reason") {
  FakeHost fake;
  fake.say(R"({"k":"x","y":"the game is full of ghosts","v":)" + std::to_string(wire::kProtocolVersion) + "}");
  REQUIRE(fake.table.until([&fake] { return fake.table.count(0, NetworkEvent::Kind::Failed) == 1; }));
  CHECK(fake.table.last(0, NetworkEvent::Kind::Failed)->name == "the game is full of ghosts");
}

namespace {

constexpr float kFacingDegrees = 45.0f;
constexpr double kAngleTolerance = 0.01;  // of the angle: within half a degree of 45
constexpr int kStillPoses = 3;

// The poses a seat heard, as the game's scripts get them.
std::vector<NetworkEvent> poses_heard(const Table& table, std::size_t seat) {
  std::vector<NetworkEvent> poses;
  for (const NetworkEvent& event : table.events[seat]) {
    if (event.kind == NetworkEvent::Kind::Message && event.name == kPoseMessageName) poses.push_back(event);
  }
  return poses;
}

}  // namespace

TEST_CASE("a pose goes to everyone else through the host, and arrives as the $sync message") {
  Party party;
  const redgem::Quat facing = redgem::quat_from_euler_degrees({0.0f, kFacingDegrees, 0.0f});
  REQUIRE(party.first.send_pose("hero-2", {1.0f, 2.0f, 3.0f}, facing));
  REQUIRE(party.table.until([&party] {
    return poses_heard(party.table, Party::kHostSeat).size() == 1 &&
           poses_heard(party.table, Party::kSecondSeat).size() == 1;
  }));
  for (const std::size_t seat : {Party::kHostSeat, Party::kSecondSeat}) {
    const NetworkEvent pose = poses_heard(party.table, seat).back();
    CHECK(pose.peer == kFirstGuest);
    CHECK(pose.data[0] == "hero-2");
    CHECK(pose.data[1][0].get<float>() == doctest::Approx(1.0f));
    CHECK(pose.data[1][2].get<float>() == doctest::Approx(3.0f));
    CHECK(pose.data[2][1].get<double>() == doctest::Approx(kFacingDegrees).epsilon(kAngleTolerance));
  }
  // Then only the move travels, and it comes out where the object is.
  REQUIRE(party.first.send_pose("hero-2", {1.5f, 2.0f, 3.0f}, facing));
  REQUIRE(party.table.until([&party] { return poses_heard(party.table, Party::kSecondSeat).size() == 2; }));
  CHECK(poses_heard(party.table, Party::kSecondSeat).back().data[1][0].get<float>() == doctest::Approx(1.5f));
  CHECK_FALSE(party.first.send_pose(std::string(kMaxPoseKeyBytes + 1, 'k'), {}, facing));
}

TEST_CASE("a machine that joins hears the others' objects whole, though they only send what changed") {
  LoneHost lone;
  NetworkSession mover, late;
  lone.table.seat(mover);
  lone.table.seat(late);
  const std::size_t late_seat = 2;
  std::string error;
  REQUIRE(mover.join(kLocalhost, lone.host.port(), error));
  REQUIRE(lone.table.until([&mover] { return mover.state() == NetworkSession::State::Online; }));
  for (int i = 0; i < kStillPoses; ++i) mover.send_pose("crate", {5.0f, 0.0f, 0.0f}, redgem::Quat::identity());
  REQUIRE(late.join(kLocalhost, lone.host.port(), error));
  REQUIRE(lone.table.until([&mover] { return mover.peers().size() == 2; }));  // it heard of the newcomer
  mover.send_pose("crate", {5.0f, 0.0f, 0.0f}, redgem::Quat::identity());
  REQUIRE(lone.table.until([&] { return poses_heard(lone.table, late_seat).size() == 1; }));
  CHECK(poses_heard(lone.table, late_seat).back().data[1][0].get<float>() == doctest::Approx(5.0f));
}
