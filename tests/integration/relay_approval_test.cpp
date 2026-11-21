// Connection approval in a room on a relay, seen from the room's opener: a raw connection that
// reads what the relay tells it, word for word.

#include <doctest/doctest.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "integration/wire_text.hpp"
#include "net/frame_buffer.hpp"
#include "net/network_session.hpp"
#include "net/rooms/relay_server.hpp"
#include "socket_rounds.hpp"

using namespace redgem::net;

namespace {

constexpr int kMaxRounds = 3000;          // of polling, each after a wait for the sockets (see socket_rounds.hpp)
constexpr float kArrivalPatience = 5.0f;  // seconds: far longer than what was written takes to be there
constexpr float kRoundSeconds = 0.001f;
constexpr const char* kLocalhost = "127.0.0.1";

// A relay, the sessions joining its room, and an opener that is a raw connection.
struct GuardedRoom {
  RelayServer relay;
  std::vector<NetworkSession*> sessions;
  Socket opener;
  FrameBuffer heard;
  std::string code;

  // A room opened by a hello that says its game decides who comes in, and nothing else.
  GuardedRoom() {
    std::string error;
    REQUIRE_MESSAGE(relay.start(0, error), error);
    opener = Socket::connect_to(kLocalhost, relay.port(), error);
    REQUIRE(opener.valid());
    REQUIRE(until([this] { return opener.connect_status() == IoStatus::Ok; }));
    std::vector<std::uint8_t> hello = redgem::test::wire_frame(R"({"k":"h","r":"new","g":true})");
    REQUIRE(until([&] { return opener.write(hello) == IoStatus::Ok && hello.empty(); }));
    code = next_message()["r"];
  }

  void poll_sessions() {
    for (NetworkSession* session : sessions) session->poll(kRoundSeconds);
  }

  bool until(const std::function<bool()>& done) {
    for (int round = 0; round < kMaxRounds; ++round) {
      relay.poll(kRoundSeconds);
      poll_sessions();
      if (done()) return true;
      wait_for_sockets();
    }
    return false;
  }

  // Polls only the sessions until `done`.
  bool sessions_until(const std::function<bool()>& done) {
    for (int round = 0; round < kMaxRounds; ++round) {
      poll_sessions();
      if (done()) return true;
      wait_for_sockets();
    }
    return false;
  }

  // Until one of the sockets of the relay or of the sessions is ready (see socket_rounds.hpp).
  void wait_for_sockets() const { redgem::test::wait_for_sessions(sessions, relay.watched()); }

  // The next message the relay sent the opener, waited for.
  nlohmann::json next_message() {
    std::optional<std::string> text;
    REQUIRE(until([&] {
      std::vector<std::uint8_t> arrived;
      opener.read(arrived, kMaxReadBytesPerPoll);
      heard.append(arrived.data(), arrived.size());
      text = heard.next();
      return text.has_value();
    }));
    return redgem::test::message_in(*text);
  }

  void join(NetworkSession& session) {
    sessions.push_back(&session);
    std::string error;
    REQUIRE_MESSAGE(session.join_room(kLocalhost, relay.port(), code, error), error);
  }
};

}  // namespace

TEST_CASE("a room whose opener's hello says its game decides is guarded from the start") {
  GuardedRoom room;
  NetworkSession guest;
  room.join(guest);
  const nlohmann::json asked = room.next_message();
  CHECK(asked["k"] == wire::kAsking);
  CHECK(asked["f"] == kFirstGuestId);
  CHECK(guest.state() == NetworkSession::State::Connecting);
}

TEST_CASE("the id of a newcomer who gives up waiting is nobody else's until the opener heard it left") {
  GuardedRoom room;
  NetworkSession leaving, next;
  room.join(leaving);
  REQUIRE(room.next_message()["f"] == kFirstGuestId);
  // The first leaves and the next one's hello is on its way: the relay hears both in one poll.
  leaving.leave();
  room.sessions.clear();
  room.join(next);
  // Its hello is written out (once its call is through) ...
  REQUIRE(room.sessions_until([&next] { return !next.watched().front().sending; }));
  // ... and it waits at the relay's door, after the goodbye (what the relay listens on is the first
  // socket a wait on it watches).
  REQUIRE(Socket::wait({room.relay.watched().front()}, kArrivalPatience));
  const nlohmann::json asked = room.next_message();
  CHECK(asked["k"] == wire::kAsking);
  CHECK(asked["f"] == kFirstGuestId + 1);
  const nlohmann::json gone = room.next_message();
  CHECK(gone["k"] == wire::kGone);
  CHECK(gone["f"] == kFirstGuestId);
}
