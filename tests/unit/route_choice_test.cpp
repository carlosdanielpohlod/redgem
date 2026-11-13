#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

#include "net/stun/route_choice.hpp"

using namespace redgem::net::stun;

namespace {

constexpr const char* kLanAddress = "192.168.0.12";
constexpr const char* kPublicAddress = "203.0.113.7";
constexpr const char* kRelayAddress = "relay.example.com";
constexpr std::uint16_t kGamePort = 7777;
constexpr std::uint16_t kPublicPort = 54321;
constexpr std::uint16_t kRelayPort = 7778;
constexpr float kFrame = 0.125f;  // a power of two: the frames add up to the chooser's times exactly
constexpr int kFramesPerDirectTry = static_cast<int>(kDirectTrySeconds / kFrame);
constexpr int kFramesPerRelayTry = static_cast<int>(kRelayTrySeconds / kFrame);
constexpr float kHitchSeconds = 2 * kRelayTrySeconds;  // a frame that took far longer than any try

// A dialer that answers each address as the test says: Trying (silent, as a router that drops
// what nobody asked for), Connected or Refused.
class FakeDialer final : public Dialer {
 public:
  void dial(const Route& route) override { dialed.push_back(route); }
  DialStatus status() override {
    const auto answer = answers.find(dialed.back().address);
    return answer == answers.end() ? DialStatus::Trying : answer->second;
  }

  std::vector<Route> dialed;
  std::map<std::string, DialStatus> answers;
};

std::vector<Route> direct_routes() {
  return {Route{RouteKind::Direct, kLanAddress, kGamePort}, Route{RouteKind::Direct, kPublicAddress, kPublicPort}};
}

Route relay_route() { return Route{RouteKind::Relay, kRelayAddress, kRelayPort}; }

void run_frames(RouteChooser& chooser, int frames) {
  for (int frame = 0; frame < frames; ++frame) chooser.update(kFrame);
}

}  // namespace

TEST_CASE("the first direct route is dialed at once") {
  FakeDialer dialer;
  const RouteChooser chooser(dialer, direct_routes(), relay_route());
  REQUIRE(dialer.dialed.size() == 1);
  CHECK(dialer.dialed[0].address == kLanAddress);
  CHECK(chooser.state() == ChoiceState::Trying);
  CHECK_FALSE(chooser.through_relay());
}

TEST_CASE("a direct route that connects is kept") {
  FakeDialer dialer;
  dialer.answers[kLanAddress] = DialStatus::Connected;
  RouteChooser chooser(dialer, direct_routes(), relay_route());
  chooser.update(kFrame);
  CHECK(chooser.state() == ChoiceState::Connected);
  CHECK(chooser.route().address == kLanAddress);
  CHECK(dialer.dialed.size() == 1);
}

TEST_CASE("a refused direct route moves on to the next one at once") {
  FakeDialer dialer;
  dialer.answers[kLanAddress] = DialStatus::Refused;
  dialer.answers[kPublicAddress] = DialStatus::Connected;
  RouteChooser chooser(dialer, direct_routes(), relay_route());
  chooser.update(kFrame);
  chooser.update(kFrame);
  CHECK(chooser.state() == ChoiceState::Connected);
  CHECK(chooser.route().address == kPublicAddress);
  CHECK(chooser.route().port == kPublicPort);
}

TEST_CASE("a silent direct route is given up on in time, and the relay carries the game") {
  FakeDialer dialer;
  dialer.answers[kRelayAddress] = DialStatus::Connected;
  RouteChooser chooser(dialer, direct_routes(), relay_route());
  run_frames(chooser, kFramesPerDirectTry - 1);
  CHECK(dialer.dialed.size() == 1);
  run_frames(chooser, 1);
  CHECK(dialer.dialed.size() == 2);
  run_frames(chooser, kFramesPerDirectTry + 1);
  CHECK(chooser.state() == ChoiceState::Connected);
  CHECK(chooser.through_relay());
  CHECK(chooser.route().address == kRelayAddress);
}

TEST_CASE("with no direct routes the relay is dialed at once") {
  FakeDialer dialer;
  const RouteChooser chooser(dialer, {}, relay_route());
  REQUIRE(dialer.dialed.size() == 1);
  CHECK(chooser.through_relay());
}

TEST_CASE("the choice fails when the relay is refused too") {
  FakeDialer dialer;
  dialer.answers[kLanAddress] = DialStatus::Refused;
  dialer.answers[kPublicAddress] = DialStatus::Refused;
  dialer.answers[kRelayAddress] = DialStatus::Refused;
  RouteChooser chooser(dialer, direct_routes(), relay_route());
  run_frames(chooser, static_cast<int>(direct_routes().size()) + 1);
  CHECK(chooser.state() == ChoiceState::Failed);
  const std::size_t dialed = dialer.dialed.size();
  run_frames(chooser, 1);
  CHECK(dialer.dialed.size() == dialed);
}

TEST_CASE("a silent relay is given up on after its own, longer time") {
  FakeDialer dialer;
  RouteChooser chooser(dialer, {}, relay_route());
  run_frames(chooser, kFramesPerRelayTry - 1);
  CHECK(chooser.state() == ChoiceState::Trying);
  run_frames(chooser, 1);
  CHECK(chooser.state() == ChoiceState::Failed);
}

TEST_CASE("one long frame does not use up a direct route's time") {
  FakeDialer dialer;
  RouteChooser chooser(dialer, direct_routes(), relay_route());
  chooser.update(kHitchSeconds);
  CHECK(dialer.dialed.size() == 1);
  CHECK(chooser.state() == ChoiceState::Trying);
}
