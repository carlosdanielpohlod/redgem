// Movement validation in a game: the host does not believe the moves of the others' objects past
// the game's limits, everyone sees them where the host believes them, and their owner is told.

#include <doctest/doctest.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "net/network_session.hpp"

using namespace redgem::net;

namespace {

constexpr int kMaxRounds = 3000;  // of polling, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr float kRoundSeconds = 0.001f;
constexpr const char* kLocalhost = "127.0.0.1";
constexpr int kMover = kHostId + 1;
constexpr const char* kHero = "hero-2";
constexpr float kTeleport = 5.0f;  // meters a pose may move an object
constexpr float kFar = 80.0f;      // where a cheat puts it
constexpr float kNearby = 1.0f;    // where it goes next, a fair move from where it was put back
constexpr std::size_t kHostSeat = 0, kMoverSeat = 1, kWatcherSeat = 2, kSeats = 3;

// A host and two guests in a game, everything that happened to each, polled together.
struct Game {
  NetworkSession host, mover, watcher;
  std::vector<NetworkSession*> sessions{&host, &mover, &watcher};
  std::vector<std::vector<NetworkEvent>> events = std::vector<std::vector<NetworkEvent>>(kSeats);

  Game() {
    std::string error;
    REQUIRE_MESSAGE(host.host(0, error), error);
    for (NetworkSession* guest : {&mover, &watcher}) {
      REQUIRE_MESSAGE(guest->join(kLocalhost, host.port(), error), error);
      REQUIRE(until([guest] { return guest->state() == NetworkSession::State::Online; }));
    }
    REQUIRE(until([this] { return host.peers().size() == 2 && mover.peers().size() == 2; }));
  }

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

  std::vector<NetworkEvent> heard(std::size_t seat, const char* name) const {
    std::vector<NetworkEvent> found;
    for (const NetworkEvent& event : events[seat]) {
      if (event.kind == NetworkEvent::Kind::Message && event.name == name) found.push_back(event);
    }
    return found;
  }

  // Where the last pose a seat heard put the hero, along x.
  float hero_x(std::size_t seat) const { return heard(seat, kPoseMessageName).back().data[1][0].get<float>(); }

  // The mover sends its hero's pose, and waits for the host and the watcher to hear one more.
  void move_to(float x) {
    const std::size_t before = heard(kWatcherSeat, kPoseMessageName).size();
    REQUIRE(mover.send_pose(kHero, {x, 0.0f, 0.0f}, redgem::Quat::identity()));
    REQUIRE(until([&] {
      return heard(kWatcherSeat, kPoseMessageName).size() > before &&
             heard(kHostSeat, kPoseMessageName).size() > before;
    }));
  }

  bool any(std::size_t seat, NetworkEvent::Kind kind) const {
    for (const NetworkEvent& event : events[seat]) {
      if (event.kind == kind) return true;
    }
    return false;
  }
};

MovementRules teleport_rules(int kick_after = 0) {
  MovementRules rules;
  rules.teleport_distance = kTeleport;
  rules.kick_after = kick_after;
  return rules;
}

}  // namespace

TEST_CASE("without movement validation every move is believed, as before") {
  Game game;
  CHECK_FALSE(game.host.movement_rules().enabled());
  game.move_to(0.0f);
  game.move_to(kFar);
  for (const std::size_t seat : {kHostSeat, kWatcherSeat}) CHECK(game.hero_x(seat) == doctest::Approx(kFar));
  CHECK(game.heard(kMoverSeat, kCorrectionMessageName).empty());
  CHECK(game.host.movement_violations(kMover) == 0);
}

TEST_CASE("a teleport is not believed: everyone sees the object where it was, and its machine is told") {
  Game game;
  game.host.validate_movement(teleport_rules());
  game.move_to(0.0f);
  game.move_to(kFar);
  for (const std::size_t seat : {kHostSeat, kWatcherSeat}) CHECK(game.hero_x(seat) == doctest::Approx(0.0f));
  REQUIRE(game.until([&game] { return !game.heard(kMoverSeat, kCorrectionMessageName).empty(); }));
  const NetworkEvent correction = game.heard(kMoverSeat, kCorrectionMessageName).back();
  CHECK(correction.peer == kHostId);
  CHECK(correction.data[0] == kHero);
  CHECK(correction.data[1][0].get<float>() == doctest::Approx(0.0f));
  CHECK(game.host.movement_violations(kMover) == 1);
  // Put back, the object moves on from there, and the others see it where its machine has it again.
  game.move_to(kNearby);
  for (const std::size_t seat : {kHostSeat, kWatcherSeat}) CHECK(game.hero_x(seat) == doctest::Approx(kNearby));
  game.move_to(kNearby + kNearby);
  CHECK(game.hero_x(kWatcherSeat) == doctest::Approx(kNearby + kNearby));
  CHECK(game.host.movement_violations(kMover) == 1);
}

TEST_CASE("a machine past the violations the game allows is let go") {
  Game game;
  game.host.validate_movement(teleport_rules(1));
  game.move_to(0.0f);
  REQUIRE(game.mover.send_pose(kHero, {kFar, 0.0f, 0.0f}, redgem::Quat::identity()));
  REQUIRE(game.until([&game] { return game.any(kHostSeat, NetworkEvent::Kind::PeerLeft); }));
  CHECK(game.until([&game] { return game.mover.state() == NetworkSession::State::Offline; }));
  // Gone, its count is forgotten with it.
  CHECK(game.host.movement_violations(kMover) == 0);
}
