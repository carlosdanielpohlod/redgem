// Reconnecting after a drop (NetworkSession::allow_reconnect): a guest whose connection fails gets
// its place and id back within the host's window, with the token its welcome gave it, and nobody
// else gets them. Connections fail for real (a reset, through a Middleman); time passes as the
// polls say, so a window is crossed in one poll.

#include <doctest/doctest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

#include "integration/middleman.hpp"
#include "integration/wire_text.hpp"
#include "net/network_session.hpp"
#include "net/resume_token.hpp"
#include "test_support.hpp"

using namespace redgem::net;
using redgem::test::Middleman;

namespace {

constexpr int kMaxRounds = 3000;  // of polling, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr float kRoundSeconds = 0.001f;
constexpr float kWindow = 20.0f;               // seconds the host holds a place
constexpr float kPastWindow = kWindow + 1.0f;  // ... and a poll that crosses it
constexpr const char* kLocalhost = "127.0.0.1";
constexpr int kFirstGuest = kFirstGuestId;
constexpr int kSecondGuest = kFirstGuestId + 1;
constexpr std::size_t kReadBytes = 64 * 1024;
constexpr int kSettleRounds = 50;

using Kind = NetworkEvent::Kind;
using State = NetworkSession::State;

// Sessions on this machine, the middlemen between them, and everything that happened to each.
struct Table {
  std::vector<NetworkSession*> sessions;
  std::vector<std::vector<NetworkEvent>> events;
  std::vector<Middleman*> middlemen;

  std::size_t seat(NetworkSession& session) {
    sessions.push_back(&session);
    events.emplace_back();
    return sessions.size() - 1;
  }

  void poll_all(float seconds) {
    for (Middleman* middleman : middlemen) middleman->pump();
    for (std::size_t i = 0; i < sessions.size(); ++i) {
      for (NetworkEvent& event : sessions[i]->poll(seconds)) events[i].push_back(std::move(event));
    }
  }

  // Polls everyone until `done` (false if it never is).
  bool until(const std::function<bool()>& done) {
    for (int round = 0; round < kMaxRounds; ++round) {
      poll_all(kRoundSeconds);
      if (done()) return true;
      std::this_thread::sleep_for(kRoundPause);
    }
    return false;
  }

  // Polls everyone a while, for anything still on its way to arrive.
  void settle() {
    int rounds = 0;
    until([&rounds] { return ++rounds == kSettleRounds; });
  }

  int count(std::size_t seat_index, Kind kind) const {
    int total = 0;
    for (const NetworkEvent& event : events[seat_index]) total += event.kind == kind ? 1 : 0;
    return total;
  }

  const NetworkEvent* last(std::size_t seat_index, Kind kind) const {
    for (auto event = events[seat_index].rbegin(); event != events[seat_index].rend(); ++event) {
      if (event->kind == kind) return &*event;
    }
    return nullptr;
  }
};

// A host that holds places, a guest that reaches it through a middleman (whose connection the
// test cuts), and a guest that reaches it directly.
struct Party {
  NetworkSession host, dropping, staying;
  Table table;
  std::unique_ptr<Middleman> network;
  std::size_t host_seat = 0, dropping_seat = 0, staying_seat = 0;

  explicit Party(float window = kWindow) {
    std::string error;
    host.allow_reconnect(window);
    REQUIRE_MESSAGE(host.host(0, error), error);
    network = std::make_unique<Middleman>(host.port());
    REQUIRE(network->valid());
    table.middlemen.push_back(network.get());
    host_seat = table.seat(host);
    dropping_seat = table.seat(dropping);
    staying_seat = table.seat(staying);
    REQUIRE_MESSAGE(dropping.join(kLocalhost, network->port(), error), error);
    REQUIRE(table.until([this] { return dropping.state() == State::Online; }));
    REQUIRE_MESSAGE(staying.join(kLocalhost, host.port(), error), error);
    REQUIRE(table.until([this] { return staying.state() == State::Online && dropping.peers().size() == 2; }));
  }

  // The dropping guest's connection fails, and stays out of reach until let through again: both
  // the host and the guest have noticed by then.
  void drop() {
    network->block(true);
    network->cut();
    REQUIRE(table.until(
        [this] { return dropping.state() == State::Reconnecting && table.count(host_seat, Kind::Reconnecting) == 1; }));
  }

  void let_through() { network->block(false); }

  bool back() {
    return table.until([this] { return dropping.state() == State::Online; });
  }
};

// Something that is not this engine, talking to a host over a plain connection: what a machine
// that stole or made up a token would send.
struct Stranger {
  Socket socket;
  Table& table;

  Stranger(std::uint16_t port, Table& host_table) : table(host_table) {
    std::string error;
    socket = Socket::connect_to(kLocalhost, port, error);
    REQUIRE(socket.valid());
    REQUIRE(table.until([this] { return socket.connect_status() == IoStatus::Ok; }));
  }

  void say(const nlohmann::json& message) {
    std::vector<std::uint8_t> bytes = wire::packed(message);
    REQUIRE(table.until([&] { return socket.write(bytes) == IoStatus::Ok && bytes.empty(); }));
  }

  // The next message the host sent it, waited for (null when none came).
  nlohmann::json hear() {
    FrameBuffer heard;
    std::optional<std::string> bytes;
    table.until([&] {
      std::vector<std::uint8_t> arrived;
      socket.read(arrived, kReadBytes);
      heard.append(arrived.data(), arrived.size());
      bytes = heard.next();
      return bytes.has_value();
    });
    return bytes ? redgem::test::message_in(*bytes) : nlohmann::json();
  }

  bool hung_up_on() {
    return table.until([this] {
      std::vector<std::uint8_t> ignored;
      return socket.read(ignored, kReadBytes) == IoStatus::Closed;
    });
  }
};

bool refused_for_its_token(const nlohmann::json& answer) {
  return answer.value(wire::kKind, std::string()) == wire::kRefused &&
         answer.value(wire::kReason, std::string()) == kResumeRefused;
}

}  // namespace

TEST_CASE("a guest whose connection drops gets its place and id back within the window") {
  Party party;
  party.drop();
  // Meanwhile it is still in the game, as itself, with the same others.
  CHECK(party.dropping.id() == kFirstGuest);
  CHECK(party.dropping.peers() == std::vector<int>{kHostId, kSecondGuest});
  CHECK_FALSE(party.dropping.send("lost", 1));
  CHECK(party.table.last(party.host_seat, Kind::Reconnecting)->peer == kFirstGuest);
  CHECK(party.table.last(party.dropping_seat, Kind::Reconnecting)->peer == kFirstGuest);
  CHECK(party.host.peers() == std::vector<int>{kFirstGuest, kSecondGuest});

  party.let_through();
  REQUIRE(party.back());
  REQUIRE(party.table.until([&party] { return party.table.count(party.host_seat, Kind::Reconnected) == 1; }));
  CHECK(party.dropping.id() == kFirstGuest);
  CHECK(party.table.last(party.dropping_seat, Kind::Reconnected)->peer == kFirstGuest);
  CHECK(party.table.last(party.host_seat, Kind::Reconnected)->peer == kFirstGuest);
  // Nobody heard it leave, and nothing ended for it.
  party.table.settle();
  CHECK(party.table.count(party.host_seat, Kind::PeerLeft) == 0);
  CHECK(party.table.count(party.staying_seat, Kind::PeerLeft) == 0);
  CHECK(party.table.count(party.dropping_seat, Kind::Disconnected) == 0);
  CHECK(party.table.count(party.dropping_seat, Kind::Lost) == 0);
  CHECK(party.table.count(party.dropping_seat, Kind::Connected) == 1);
}

TEST_CASE("after getting back in, messages go both ways as before, to the same ids") {
  Party party;
  party.drop();
  party.let_through();
  REQUIRE(party.back());
  CHECK(party.dropping.send("again", 1));
  CHECK(party.staying.send("welcome back", 1, kFirstGuest));
  REQUIRE(party.table.until([&party] {
    return party.table.count(party.host_seat, Kind::Message) == 1 &&
           party.table.count(party.staying_seat, Kind::Message) == 1 &&
           party.table.count(party.dropping_seat, Kind::Message) == 1;
  }));
  CHECK(party.table.last(party.staying_seat, Kind::Message)->peer == kFirstGuest);
  CHECK(party.table.last(party.dropping_seat, Kind::Message)->peer == kSecondGuest);
  CHECK(party.table.last(party.dropping_seat, Kind::Message)->name == "welcome back");
}

TEST_CASE("a guest back after the others came and went knows who is in the game now") {
  Party party;
  const int joined_before = party.table.count(party.dropping_seat, Kind::PeerJoined);
  party.drop();
  party.staying.leave();
  REQUIRE(party.table.until([&party] { return party.host.peers() == std::vector<int>{kFirstGuest}; }));
  NetworkSession newcomer;
  const std::size_t newcomer_seat = party.table.seat(newcomer);
  std::string error;
  REQUIRE(newcomer.join(kLocalhost, party.host.port(), error));
  REQUIRE(party.table.until([&newcomer] { return newcomer.state() == State::Online; }));
  CHECK(newcomer.id() == kSecondGuest);  // the id that was free: the held one is not

  party.let_through();
  REQUIRE(party.back());
  CHECK(party.dropping.id() == kFirstGuest);
  CHECK(party.dropping.peers() == std::vector<int>{kHostId, kSecondGuest});
  CHECK(party.host.peers() == std::vector<int>{kFirstGuest, kSecondGuest});
  CHECK(newcomer.peers() == std::vector<int>{kHostId, kFirstGuest});
  // The one that left and the one that came are the same id: it heard both.
  CHECK(party.table.count(party.dropping_seat, Kind::PeerLeft) == 1);
  CHECK(party.table.count(party.dropping_seat, Kind::PeerJoined) == joined_before + 1);
  CHECK(party.table.count(newcomer_seat, Kind::PeerLeft) == 0);
}

TEST_CASE("a guest that does not get back within the window loses the game, and the others hear it left") {
  Party party;
  party.drop();
  party.table.poll_all(kPastWindow);
  REQUIRE(party.table.until([&party] { return party.table.count(party.staying_seat, Kind::PeerLeft) == 1; }));
  CHECK(party.table.last(party.staying_seat, Kind::PeerLeft)->peer == kFirstGuest);
  CHECK(party.host.peers() == std::vector<int>{kSecondGuest});
  CHECK(party.dropping.state() == State::Offline);
  CHECK(party.dropping.id() == 0);
  REQUIRE(party.table.count(party.dropping_seat, Kind::Lost) == 1);
  CHECK(party.table.last(party.dropping_seat, Kind::Lost)->name == kReconnectTimedOut);
  CHECK(party.table.count(party.dropping_seat, Kind::Disconnected) == 0);
}

TEST_CASE("a place is held only when the host says so, and never for a machine that left") {
  // No window: a drop ends the game for the one that dropped, as it always did.
  {
    Party party(0.0f);
    party.network->cut();
    REQUIRE(party.table.until([&party] { return party.table.count(party.dropping_seat, Kind::Disconnected) == 1; }));
    REQUIRE(party.table.until([&party] { return party.table.count(party.host_seat, Kind::PeerLeft) == 1; }));
    CHECK(party.table.count(party.host_seat, Kind::Reconnecting) == 0);
  }
  // A guest that leaves says so: the others hear at once, and no place is held.
  {
    Party party;
    party.dropping.leave();
    REQUIRE(party.table.until([&party] { return party.table.count(party.staying_seat, Kind::PeerLeft) == 1; }));
    CHECK(party.table.count(party.host_seat, Kind::Reconnecting) == 0);
  }
  // A host that leaves ends the game: nobody calls back.
  {
    Party party;
    party.host.leave();
    REQUIRE(party.table.until([&party] { return party.table.count(party.dropping_seat, Kind::Disconnected) == 1; }));
    CHECK(party.table.count(party.dropping_seat, Kind::Reconnecting) == 0);
    CHECK(party.dropping.state() == State::Offline);
  }
}

namespace {

// A host that holds places, and a stranger in its game that knows its own token.
struct HeldPlace {
  NetworkSession host;
  Table table;
  std::unique_ptr<Stranger> holder;
  std::string token;

  HeldPlace() {
    std::string error;
    host.allow_reconnect(kWindow);
    REQUIRE(host.host(0, error));
    table.seat(host);
    holder = std::make_unique<Stranger>(host.port(), table);
    holder->say(wire::hello());
    const nlohmann::json welcome = holder->hear();
    REQUIRE(welcome.value(wire::kKind, std::string()) == wire::kWelcome);
    CHECK(welcome.value(wire::kWindow, 0.0f) == kWindow);
    token = welcome.value(wire::kResume, std::string());
    REQUIRE(token.size() == kResumeTokenDigits);
  }

  // The holder's connection fails, and the host holds its place.
  void drop_holder() {
    holder->socket.reset();
    REQUIRE(table.until([this] { return table.count(0, Kind::Reconnecting) == 1; }));
  }

  // A new connection that asks for the place with `offered`: what the host answers.
  nlohmann::json ask_back(const std::string& offered, std::unique_ptr<Stranger>& asking) {
    asking = std::make_unique<Stranger>(host.port(), table);
    asking->say(wire::resume_hello(offered));
    return asking->hear();
  }
};

}  // namespace

TEST_CASE("a token gets its place back, with the same id and a new token") {
  HeldPlace held;
  held.drop_holder();
  std::unique_ptr<Stranger> back;
  const nlohmann::json welcome = held.ask_back(held.token, back);
  CHECK(welcome.value(wire::kKind, std::string()) == wire::kWelcome);
  CHECK(welcome.value(wire::kTo, 0) == kFirstGuest);
  CHECK(welcome.value(wire::kPeers, std::vector<int>{}) == std::vector<int>{kHostId});
  const std::string next = welcome.value(wire::kResume, std::string());
  CHECK(next.size() == kResumeTokenDigits);
  CHECK(next != held.token);
  CHECK(held.host.peers() == std::vector<int>{kFirstGuest});
}

TEST_CASE("a token offered after the window is over is refused, and the place is gone") {
  HeldPlace held;
  held.drop_holder();
  held.table.poll_all(kPastWindow);
  CHECK(held.host.peers().empty());
  CHECK(held.table.count(0, Kind::PeerLeft) == 1);
  std::unique_ptr<Stranger> late;
  CHECK(refused_for_its_token(held.ask_back(held.token, late)));
  CHECK(late->hung_up_on());
  CHECK(held.host.peers().empty());
}

TEST_CASE("a made-up, empty or used token is refused, and the place stays with its holder") {
  HeldPlace held;
  for (const std::string& made_up : {new_resume_token(), std::string(), std::string(kResumeTokenDigits, '0')}) {
    CAPTURE(made_up);
    std::unique_ptr<Stranger> thief;
    CHECK(refused_for_its_token(held.ask_back(made_up, thief)));
    CHECK(thief->hung_up_on());
  }
  CHECK(held.host.peers() == std::vector<int>{kFirstGuest});
  CHECK(held.table.count(0, Kind::Reconnected) == 0);
  // Once used, a token is spent: whoever saw it on its way gets nothing with it.
  held.drop_holder();
  std::unique_ptr<Stranger> back;
  REQUIRE(held.ask_back(held.token, back).value(wire::kKind, std::string()) == wire::kWelcome);
  std::unique_ptr<Stranger> replay;
  CHECK(refused_for_its_token(held.ask_back(held.token, replay)));
  CHECK(held.host.peers() == std::vector<int>{kFirstGuest});
  CHECK(held.table.count(0, Kind::Reconnected) == 1);
}

TEST_CASE("a host that holds no places refuses every token") {
  NetworkSession host;
  Table table;
  std::string error;
  REQUIRE(host.host(0, error));
  table.seat(host);
  Stranger guest(host.port(), table);
  guest.say(wire::hello());
  const nlohmann::json welcome = guest.hear();
  CHECK_FALSE(welcome.contains(wire::kResume));
  Stranger thief(host.port(), table);
  thief.say(wire::resume_hello(new_resume_token()));
  CHECK(refused_for_its_token(thief.hear()));
  CHECK(host.peers() == std::vector<int>{kFirstGuest});
}

TEST_CASE("a token that comes back before the host noticed the drop takes its place over") {
  HeldPlace held;
  std::unique_ptr<Stranger> back;
  const nlohmann::json welcome = held.ask_back(held.token, back);
  CHECK(welcome.value(wire::kTo, 0) == kFirstGuest);
  CHECK(held.holder->hung_up_on());  // the old connection is let go, without a goodbye
  held.table.settle();
  CHECK(held.host.peers() == std::vector<int>{kFirstGuest});
  CHECK(held.table.count(0, Kind::PeerLeft) == 0);
  CHECK(held.table.count(0, Kind::Reconnected) == 1);
}
