// NetworkSession: what a machine that joined a game hears from its host (or from the relay its
// room is on), and how it speaks to the others through it.

#include "net/network_session.hpp"

#include <algorithm>

#include "core/log.hpp"

namespace redgem::net {

using namespace wire;

void NetworkSession::await_connection(float seconds, std::vector<NetworkEvent>& events) {
  // Connected to the host, the game starts when its welcome arrives (accept_welcome); refused, the
  // link closes and the game ends with it (end_game_if_host_gone).
  connecting_for_ += seconds;
  if (connecting_for_ <= kJoinTimeoutSeconds) return;
  go_offline();
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::Failed));
}

void NetworkSession::handle_as_guest(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  if (state_ == State::Connecting) {
    hear_while_joining(message, events);
  } else if (state_ == State::Online) {
    hear_of_the_game(message, events);
  }
}

void NetworkSession::hear_of_the_game(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  const std::string kind = message[kKind];
  if (kind == kArrived) peer_arrived(message.value(kFrom, 0), events);
  if (kind == kGone) peer_gone(message.value(kFrom, 0), events);
  if (kind == kAsking) hear_asking(message, events);
  if (is_game_message(message)) hear_from_game(message, events);
}

void NetworkSession::hear_while_joining(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  const std::string kind = message[kKind];
  // The host's game takes its time to decide on this machine: this machine waits for it.
  if (kind == kConsidering) connecting_for_ = -kApprovalSeconds;
  if (kind == kRefused) turned_away(message);
  accept_welcome(message, events);
}

void NetworkSession::turned_away(const nlohmann::json& refusal) {
  if (speaks_our_protocol(refusal)) {
    refusal_ = refusal.value(kReason, std::string());
  } else {
    refusal_ = protocol_mismatch(protocol_of(refusal));
    log::warning("cannot join the game: " + refusal_);
  }
  links_.front().connection.close();  // the game ends with it, saying why (end_game_if_host_gone)
}

void NetworkSession::accept_welcome(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  if (message[kKind] != kWelcome) return;
  // (A host of another version would have refused this machine: one that welcomes it is turned away.)
  if (!speaks_our_protocol(message)) return turned_away(message);
  const int given = message.value(kTo, 0);
  // (well_formed saw to the list: ids there can be, each once. This machine is not one of the others.)
  const std::vector<int> others = message.value(kPeers, std::vector<int>{});
  if (!believable_welcome(given, others)) {
    links_.front().connection.close();
    return;
  }
  id_ = given;
  peers_ = others;
  enter_game(message, events);
}

void NetworkSession::enter_game(const nlohmann::json& welcome, std::vector<NetworkEvent>& events) {
  room_ = welcome.value(kRoom, std::string());
  server_hosted_ = welcome.value(kServer, false);
  state_ = State::Online;
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::Connected, id_));
}

bool NetworkSession::believable_welcome(int given, const std::vector<int>& others) const {
  const bool listed_itself = std::find(others.begin(), others.end(), given) != others.end();
  return !listed_itself && own_id_believable(given, through_relay_, opening_room_);
}

void NetworkSession::peer_arrived(int peer, std::vector<NetworkEvent>& events) {
  // What a host says is checked like anything else.
  if (!can_arrive(peer)) return;
  poses_.sent.start_over();  // the newcomer knows none of this machine's objects
  peers_.push_back(peer);
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::PeerJoined, peer));
}

bool NetworkSession::host_gone() const { return !links_.empty() && links_.front().connection.closed(); }

void NetworkSession::end_game_if_host_gone(std::vector<NetworkEvent>& events) {
  if (!host_gone()) return;
  // The host is gone, and the game with it.
  const bool was_in = state_ == State::Online;
  NetworkEvent ended = NetworkEvent::of(was_in ? NetworkEvent::Kind::Disconnected : NetworkEvent::Kind::Failed);
  ended.name = refusal_;
  go_offline();
  events.push_back(ended);
}

bool NetworkSession::send_through_host(const nlohmann::json& message, int to) {
  // Everything goes to the host (or the relay), which passes it on.
  if (!can_reach(to)) return false;
  links_.front().connection.send(message);
  return true;
}

}  // namespace redgem::net
