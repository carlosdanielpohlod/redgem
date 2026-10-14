// RelayServer: who may connect. A relay faces the internet, so what one address may hold and
// try is limited, and an address that guesses at room codes is kept out for a while.

#include "net/relay_server.hpp"

#include <algorithm>
#include <iterator>

namespace redgem::net {

namespace {

// What one address may hold. (Players in one house share an address: a whole party fits.)
constexpr int kMaxConnectionsPerAddress = 24;
// Wrong guesses at a room code before an address is kept out: a whole party behind one address may
// each get the code wrong twice (someone read it out badly). Guessing a room takes millions more.
constexpr float kMaxWrongCodes = 2.0f * kMaxPeers;
constexpr float kKeptOutSeconds = 120.0f;         // kept out for this long
constexpr float kForgetWrongCodeSeconds = 30.0f;  // one wrong guess is forgotten after this
// What may wait to be sent to one machine: two of the longest messages. A relay holds many
// machines' queues at once, so it is far stricter than a game is with its few.
constexpr std::size_t kMaxPendingPerMember = 2 * kMaxFrameBytes;

}  // namespace

void RelayServer::accept_newcomers() {
  if (accept_rest_ > 0.0f) return;
  Socket guest;
  IoStatus taken = listener_.accept(guest);
  for (; taken == IoStatus::Ok; taken = listener_.accept(guest)) {
    if (!room_for(guest.peer_address())) continue;  // hung up on
    auto member = std::make_unique<Member>();
    member->connection = Connection::accepted(std::move(guest), kMaxPendingPerMember);
    members_.push_back(std::move(member));
  }
  // Someone is waiting and the system has no connection to give: asked again after a rest.
  if (taken == IoStatus::Closed) accept_rest_ = kAcceptRestSeconds;
}

bool RelayServer::room_for(const std::string& address) const {
  const auto visitor = visitors_.find(address);
  const bool kept_out = visitor != visitors_.end() && visitor->second.kept_out();
  // Not when full, kept out for guessing, or with too many from one place already.
  return members_.size() < kMaxRelayConnections && !kept_out && connections_from(address) < kMaxConnectionsPerAddress;
}

int RelayServer::connections_from(const std::string& address) const {
  return static_cast<int>(std::count_if(members_.begin(), members_.end(),
                                        [&address](const auto& member) { return member->connected_from(address); }));
}

void RelayServer::forgive_visitors(float seconds) {
  for (auto visitor = visitors_.begin(); visitor != visitors_.end();) {
    Visitor& record = visitor->second;
    record.kept_out_for = std::max(0.0f, record.kept_out_for - seconds);
    record.wrong_codes = std::max(0.0f, record.wrong_codes - seconds / kForgetWrongCodeSeconds);
    visitor = record.forgiven() ? visitors_.erase(visitor) : std::next(visitor);
  }
}

void RelayServer::note_wrong_code(Member& member) {
  // A wrong code may be a typo; many are someone trying every code there is.
  Visitor& visitor = visitors_[member.connection.address()];
  visitor.wrong_codes += 1.0f;
  if (visitor.wrong_codes >= kMaxWrongCodes) visitor.kept_out_for = kKeptOutSeconds;
  member.connection.close();
}

}  // namespace redgem::net
