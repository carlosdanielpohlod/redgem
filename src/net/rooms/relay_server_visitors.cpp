// RelayServer: who may connect. A relay faces the internet, so what one address may hold and
// try is limited, and an address that guesses at room codes is kept out for a while.

#include "net/rooms/relay_server.hpp"

#include <algorithm>

namespace redgem::net {

void RelayServer::accept_newcomers() {
  if (accept_rest_ > 0.0f) return;
  Socket guest;
  IoStatus taken = listener_.accept(guest);
  for (; taken == IoStatus::Ok; taken = listener_.accept(guest)) {
    if (!room_for(guest.peer_address())) continue;  // hung up on
    auto member = std::make_unique<Member>();
    member->connection = Connection::accepted(std::move(guest), kMaxPendingPerServed);
    members_.push_back(std::move(member));
  }
  // Someone is waiting and the system has no connection to give: asked again after a rest.
  if (taken == IoStatus::Closed) accept_rest_ = kAcceptRestSeconds;
}

bool RelayServer::room_for(const std::string& address) const {
  // Not when full, kept out for guessing, or with too many from one place already.
  return members_.size() < kMaxRelayConnections && !guessers_.keeps_out(address) &&
         connections_from(address) < kMaxConnectionsPerAddress;
}

int RelayServer::connections_from(const std::string& address) const {
  return static_cast<int>(std::count_if(members_.begin(), members_.end(),
                                        [&address](const auto& member) { return member->connected_from(address); }));
}

void RelayServer::note_wrong_code(Member& member) {
  // A wrong code may be a typo; many are someone trying every code there is.
  guessers_.note_wrong_code(member.connection.address());
  member.connection.close();
}

}  // namespace redgem::net
