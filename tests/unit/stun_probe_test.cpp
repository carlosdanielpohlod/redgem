#include <doctest/doctest.h>

#include <deque>
#include <string>
#include <vector>

#include "net/stun/stun_probe.hpp"
#include "net/stun/stun_responder.hpp"

using namespace redgem::net::stun;

namespace {

constexpr const char* kServerAddress = "198.51.100.20";
constexpr std::uint16_t kServerPort = 7778;
constexpr const char* kPublicAddress = "203.0.113.7";
constexpr std::uint16_t kPublicPort = 54321;
constexpr const char* kStrangerAddress = "192.0.2.99";
constexpr std::uint16_t kStrangerPort = 4444;
constexpr const char* kNotStun = "who is hosting?";
constexpr float kFrame = 0.125f;  // a power of two: the frames add up to the probe's times exactly
// Frames until a probe asks again, and until it gives up.
constexpr int kFramesPerRetry = static_cast<int>(kProbeRetrySeconds / kFrame);
constexpr int kFramesToGiveUp = static_cast<int>(kProbeGiveUpSeconds / kFrame);
constexpr std::size_t kFlood = 3 * kMostReadsPerPoll;
constexpr float kHitchSeconds = 2 * kProbeGiveUpSeconds;  // a frame that took far longer than the probe's time
// Where no answer goes, whatever a request says: "this network", broadcast, multicast, no port.
constexpr const char* kThisNetwork = "0.1.2.3";
constexpr const char* kBroadcast = "255.255.255.255";
constexpr const char* kMulticast = "239.1.2.3";
constexpr const char* kNotNumbers = "relay.example.com";

// A link whose datagrams the test hands in and takes out.
class FakeLink final : public DatagramLink {
 public:
  bool send(const Datagram& datagram) override {
    sent.push_back(datagram);
    return true;
  }

  std::optional<Datagram> receive() override {
    if (arriving.empty()) return std::nullopt;
    Datagram next = arriving.front();
    arriving.pop_front();
    return next;
  }

  std::vector<Datagram> sent;
  std::deque<Datagram> arriving;
};

MappedAddress server() { return {kServerAddress, kServerPort}; }

// What the server says to the request `probe_link` sent last: "you are at the public address".
Datagram answer_to_last(const FakeLink& probe_link, const MappedAddress& from = server()) {
  TransactionId id{};
  REQUIRE(is_binding_request(probe_link.sent.back().bytes, id));
  return Datagram{binding_success(id, MappedAddress{kPublicAddress, kPublicPort}), from.address, from.port};
}

void run_frames(StunProbe& probe, int frames) {
  for (int frame = 0; frame < frames; ++frame) probe.update(kFrame);
}

}  // namespace

TEST_CASE("a probe asks the server at once") {
  FakeLink link;
  StunProbe probe(link, server(), random_transaction_id());
  probe.update(kFrame);
  REQUIRE(link.sent.size() == 1);
  CHECK(link.sent[0].address == kServerAddress);
  CHECK(link.sent[0].port == kServerPort);
  CHECK(probe.state() == ProbeState::Asking);
}

TEST_CASE("a probe takes the server's answer") {
  FakeLink link;
  StunProbe probe(link, server(), random_transaction_id());
  probe.update(kFrame);
  link.arriving.push_back(answer_to_last(link));
  probe.update(kFrame);
  REQUIRE(probe.state() == ProbeState::Found);
  CHECK(probe.found() == MappedAddress{kPublicAddress, kPublicPort});
}

TEST_CASE("a probe asks again while no answer comes, then gives up") {
  FakeLink link;
  StunProbe probe(link, server(), random_transaction_id());
  probe.update(kFrame);
  run_frames(probe, kFramesPerRetry);
  CHECK(link.sent.size() == 2);
  run_frames(probe, kFramesToGiveUp);
  CHECK(probe.state() == ProbeState::Failed);
  const std::size_t asked = link.sent.size();
  run_frames(probe, kFramesPerRetry);
  CHECK(link.sent.size() == asked);
}

TEST_CASE("a probe ignores answers from anyone but its server, and what is no answer") {
  FakeLink link;
  StunProbe probe(link, server(), random_transaction_id());
  probe.update(kFrame);
  link.arriving.push_back(answer_to_last(link, MappedAddress{kStrangerAddress, kStrangerPort}));
  link.arriving.push_back(Datagram{kNotStun, kServerAddress, kServerPort});
  link.arriving.push_back(Datagram{binding_success(random_transaction_id(), MappedAddress{kStrangerAddress, 1}),
                                   kServerAddress, kServerPort});
  probe.update(kFrame);
  CHECK(probe.state() == ProbeState::Asking);
}

TEST_CASE("a responder answers each request with where it came from") {
  FakeLink link;
  StunResponder responder(link);
  const TransactionId id = random_transaction_id();
  link.arriving.push_back(Datagram{binding_request(id), kPublicAddress, kPublicPort});
  link.arriving.push_back(Datagram{kNotStun, kStrangerAddress, kStrangerPort});
  CHECK(responder.poll() == 1);
  REQUIRE(link.sent.size() == 1);
  CHECK(link.sent[0].address == kPublicAddress);
  CHECK(link.sent[0].port == kPublicPort);
  CHECK(mapped_address(link.sent[0].bytes, id) == MappedAddress{kPublicAddress, kPublicPort});
}

TEST_CASE("a responder reads a bounded number of datagrams a poll") {
  FakeLink link;
  StunResponder responder(link);
  for (std::size_t i = 0; i < kFlood; ++i) {
    link.arriving.push_back(Datagram{binding_request(random_transaction_id()), kPublicAddress, kPublicPort});
  }
  CHECK(responder.poll() == kMostReadsPerPoll);
  CHECK(link.arriving.size() == kFlood - kMostReadsPerPoll);
}

TEST_CASE("a probe and a responder over one fake network agree on the address") {
  FakeLink game, relay;
  StunProbe probe(game, server(), random_transaction_id());
  StunResponder responder(relay);
  probe.update(kFrame);
  // The network carries the request to the relay from the game's public address, and back.
  relay.arriving.push_back(Datagram{game.sent.back().bytes, kPublicAddress, kPublicPort});
  responder.poll();
  game.arriving.push_back(Datagram{relay.sent.back().bytes, kServerAddress, kServerPort});
  probe.update(kFrame);
  REQUIRE(probe.state() == ProbeState::Found);
  CHECK(probe.found() == MappedAddress{kPublicAddress, kPublicPort});
}

TEST_CASE("one long first frame still asks, and does not give up") {
  FakeLink link;
  StunProbe probe(link, server(), random_transaction_id());
  probe.update(kHitchSeconds);
  CHECK(link.sent.size() == 1);
  CHECK(probe.state() == ProbeState::Asking);
}

TEST_CASE("a responder never answers toward a broadcast, multicast or made-up source") {
  FakeLink link;
  StunResponder responder(link);
  const std::string request = binding_request(random_transaction_id());
  for (const char* address : {kThisNetwork, kBroadcast, kMulticast, kNotNumbers}) {
    link.arriving.push_back(Datagram{request, address, kPublicPort});
  }
  link.arriving.push_back(Datagram{request, kPublicAddress, 0});
  CHECK(responder.poll() == 0);
  CHECK(link.sent.empty());
}
