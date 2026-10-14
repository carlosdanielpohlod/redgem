#include <doctest/doctest.h>

#include "net/network_session.hpp"

using namespace redgem::net;

namespace {

constexpr int kGuest = 3;

}  // namespace

TEST_CASE("what happens on the network is told with who it is about") {
  const NetworkEvent joined = NetworkEvent::of(NetworkEvent::Kind::PeerJoined, kGuest);
  CHECK(joined.kind == NetworkEvent::Kind::PeerJoined);
  CHECK(joined.peer == kGuest);
  CHECK(joined.name.empty());
  CHECK(NetworkEvent::of(NetworkEvent::Kind::Failed).peer == 0);
}

TEST_CASE("a message that arrived is told with who sent it, its name and what it carries") {
  const NetworkEvent said = NetworkEvent::message(kGuest, "hit", {{"damage", 2}});
  CHECK(said.kind == NetworkEvent::Kind::Message);
  CHECK(said.peer == kGuest);
  CHECK(said.name == "hit");
  CHECK(said.data["damage"] == 2);
}
