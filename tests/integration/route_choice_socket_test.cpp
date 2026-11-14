#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <thread>

#include "net/stun/route_choice.hpp"
#include "net/stun/stun_probe.hpp"
#include "net/stun/stun_responder.hpp"

using namespace redgem::net;
using namespace redgem::net::stun;

namespace {

constexpr const char* kLocalhost = "127.0.0.1";
constexpr int kMaxRounds = 3000;  // of updating, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
// What each round tells the chooser and the probe passed: their own times pass within the rounds.
constexpr float kChooserRoundSeconds = 0.01f;
constexpr float kProbeRoundSeconds = 0.001f;

// A port of this machine nobody listens on: one the system gave, then let go of.
std::uint16_t closed_port() {
  std::string error;
  Socket listener = Socket::listen_on(0, error);
  REQUIRE_MESSAGE(listener.valid(), error);
  const std::uint16_t port = listener.local_port();
  listener.close();
  return port;
}

void settle(RouteChooser& chooser) {
  for (int round = 0; round < kMaxRounds; ++round) {
    if (chooser.state() != ChoiceState::Trying) return;
    chooser.update(kChooserRoundSeconds);
    std::this_thread::sleep_for(kRoundPause);
  }
}

// Updates the probe, the server answering, until the probe is no longer asking.
void settle(StunProbe& probe, UdpStunServer& server) {
  for (int round = 0; round < kMaxRounds; ++round) {
    if (probe.state() != ProbeState::Asking) return;
    probe.update(kProbeRoundSeconds);
    server.poll();
    std::this_thread::sleep_for(kRoundPause);
  }
}

}  // namespace

TEST_CASE("a socket dialer falls back to the relay when the direct route is closed") {
  std::string error;
  Socket relay = Socket::listen_on(0, error);
  REQUIRE_MESSAGE(relay.valid(), error);
  SocketDialer dialer;
  RouteChooser chooser(dialer, {Route{RouteKind::Direct, kLocalhost, closed_port()}},
                       Route{RouteKind::Relay, kLocalhost, relay.local_port()});
  settle(chooser);
  REQUIRE(chooser.state() == ChoiceState::Connected);
  CHECK(chooser.through_relay());
  CHECK(dialer.take().valid());
}

TEST_CASE("a socket dialer keeps a direct route that connects") {
  std::string error;
  Socket host = Socket::listen_on(0, error);
  REQUIRE_MESSAGE(host.valid(), error);
  SocketDialer dialer;
  RouteChooser chooser(dialer, {Route{RouteKind::Direct, kLocalhost, host.local_port()}},
                       Route{RouteKind::Relay, kLocalhost, closed_port()});
  settle(chooser);
  REQUIRE(chooser.state() == ChoiceState::Connected);
  CHECK_FALSE(chooser.through_relay());
}

TEST_CASE("a probe learns its address from a UDP STUN server on this machine") {
  std::string error;
  UdpStunServer server;
  REQUIRE_MESSAGE(server.open(0, error), error);
  Datagrams game = Datagrams::open_on(0, error);
  REQUIRE_MESSAGE(game.valid(), error);
  UdpLink link(game);
  StunProbe probe(link, MappedAddress{kLocalhost, server.port()}, random_transaction_id());
  settle(probe, server);
  REQUIRE(probe.state() == ProbeState::Found);
  CHECK(probe.found() == MappedAddress{kLocalhost, game.local_port()});
}
