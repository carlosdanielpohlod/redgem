#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include "net/rooms/lobby.hpp"
#include "net/network_session.hpp"

using namespace redgem::net;

namespace {

constexpr int kMaxRounds = 3000;  // of polling, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr float kRoundSeconds = 0.001f;
constexpr float kPastJoinTimeout = kJoinTimeoutSeconds + 1.0f;
constexpr int kSettleRounds = 50;
// Wrong room codes an address may give (two for each machine of a full game), and more than it
// takes to be kept out, with time forgiving a little of each meanwhile.
constexpr int kPartyGuesses = 2 * kMaxPeers;
constexpr int kTooManyGuesses = kPartyGuesses + 2;
constexpr float kIdleStep = kPingEverySeconds;  // seconds: a ping each way every step, nothing else
constexpr int kIdleSteps = static_cast<int>(kServedIdleSeconds / kIdleStep) + 1;
constexpr std::chrono::milliseconds kArrivalPause{20};  // for what was written to be there to read
constexpr const char* kLocalhost = "127.0.0.1";
constexpr const char* kSomeRoom = "K7QM3P";

// A dedicated server's lobby, the sessions knocking at it, and whoever came out.
struct Door {
  Lobby lobby;
  std::vector<NetworkSession*> sessions;
  std::vector<Arrival> arrivals;
  int in_rooms = 0;  // what the rooms hold of every address

  Door() {
    std::string error;
    REQUIRE_MESSAGE(lobby.open(0, error), error);
  }

  void round(float seconds = kRoundSeconds) {
    for (NetworkSession* session : sessions) session->poll(kRoundSeconds);
    for (Arrival& arrival : lobby.poll(seconds, [this](const std::string&) { return in_rooms; })) {
      arrivals.push_back(std::move(arrival));
    }
    std::this_thread::sleep_for(kRoundPause);
  }

  bool until(const std::function<bool()>& done) {
    for (int i = 0; i < kMaxRounds; ++i) {
      round();
      if (done()) return true;
    }
    return false;
  }

  // Starts a session joining by the server's address (any room).
  void knock(NetworkSession& session) {
    std::string error;
    REQUIRE_MESSAGE(session.join(kLocalhost, lobby.port(), error), error);
    sessions.push_back(&session);
  }
};

bool offline(const NetworkSession& session) { return session.state() == NetworkSession::State::Offline; }
bool online(const NetworkSession& session) { return session.state() == NetworkSession::State::Online; }

}  // namespace

TEST_CASE("a machine that joins a server by its address asks for any room; one with a code, for that room") {
  Door door;
  NetworkSession anywhere, friends_room;
  door.knock(anywhere);
  std::string error;
  REQUIRE(friends_room.join_room(kLocalhost, door.lobby.port(), kSomeRoom, error));
  door.sessions.push_back(&friends_room);
  REQUIRE(door.until([&door] { return door.arrivals.size() == 2; }));
  std::vector<std::string> wanted{door.arrivals[0].room, door.arrivals[1].room};
  std::sort(wanted.begin(), wanted.end());
  CHECK(wanted == std::vector<std::string>{"", kSomeRoom});
}

TEST_CASE("asking a server to open a room, or saying anything but hello first, is no way in") {
  Door door;
  NetworkSession opener;
  std::string error;
  REQUIRE(opener.host_room(kLocalhost, door.lobby.port(), error));
  door.sessions.push_back(&opener);
  REQUIRE(door.until([&opener] { return offline(opener); }));
  CHECK(door.arrivals.empty());

  Socket rude = Socket::connect_to(kLocalhost, door.lobby.port(), error);
  Connection talker = Connection::calling(std::move(rude));
  talker.send(wire::game_message("hi", nullptr, kFirstGuestId, kEveryone));
  REQUIRE(door.until([&talker] {
    talker.pump();
    return talker.closed();
  }));
  CHECK(door.arrivals.empty());
}

TEST_CASE("a machine that connects to a server and keeps quiet is let go") {
  Door door;
  std::string error;
  Connection quiet = Connection::calling(Socket::connect_to(kLocalhost, door.lobby.port(), error));
  for (int i = 0; i < kSettleRounds; ++i) door.round();
  quiet.pump();
  CHECK_FALSE(quiet.closed());
  door.round(kPastJoinTimeout);
  REQUIRE(door.until([&quiet] {
    quiet.pump();
    return quiet.closed();
  }));
}

TEST_CASE("an address that keeps asking for rooms nobody has is kept out of the server") {
  Door door;
  int guesses = 0;
  for (bool kept_out = false; !kept_out; ++guesses) {
    REQUIRE(guesses <= kTooManyGuesses);
    NetworkSession guesser;
    std::string error;
    REQUIRE(guesser.join_room(kLocalhost, door.lobby.port(), kSomeRoom, error));
    door.sessions = {&guesser};
    REQUIRE(door.until([&] { return !door.arrivals.empty() || offline(guesser); }));
    kept_out = door.arrivals.empty();  // hung up on before it could say anything
    if (!kept_out) door.lobby.turn_away_guess(door.arrivals.front());
    door.arrivals.clear();
    REQUIRE(door.until([&guesser] { return offline(guesser); }));
    door.sessions.clear();
  }
  // A whole party behind one address may each get the code wrong twice before that.
  CHECK(guesses > kPartyGuesses);
  NetworkSession after;
  door.knock(after);
  REQUIRE(door.until([&after] { return offline(after); }));
  CHECK(door.arrivals.empty());
}

TEST_CASE("an address whose machines fill what one address may hold is hung up on") {
  Door door;
  door.in_rooms = kMaxConnectionsPerAddress;
  NetworkSession one_more;
  door.knock(one_more);
  REQUIRE(door.until([&one_more] { return offline(one_more); }));
  CHECK(door.arrivals.empty());
}

TEST_CASE("a server's room lets go of a player who says nothing to the game for minutes, and keeps one who plays") {
  Door door;
  NetworkSession room, idle, playing;
  room.serve_room(kSomeRoom);
  door.knock(idle);
  door.knock(playing);
  REQUIRE(door.until([&door] { return door.arrivals.size() == 2; }));
  for (Arrival& arrival : door.arrivals) room.admit_arrival(std::move(arrival.connection));
  door.sessions.push_back(&room);
  REQUIRE(door.until([&] { return online(idle) && online(playing); }));
  const auto play_for = [&](int steps) {
    for (int step = 0; step < steps; ++step) {
      playing.send("move", step);
      idle.poll(kIdleStep);
      playing.poll(kIdleStep);
      std::this_thread::sleep_for(kArrivalPause);
      room.poll(kIdleStep);
    }
  };
  play_for(kIdleSteps - 2);
  REQUIRE(door.until([] { return true; }));
  CHECK(online(idle));  // only "I am here" for nearly as long as a player may be idle: not over yet
  play_for(2);
  REQUIRE(door.until([&idle] { return offline(idle); }));
  CHECK(online(playing));
  CHECK(room.players() == std::vector<int>{playing.id()});
}
