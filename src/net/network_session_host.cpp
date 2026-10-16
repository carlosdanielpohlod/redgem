// NetworkSession: what the machine hosting a game does for the ones that joined it. It lets them
// in, passes their messages on, and tells the others who came and went.

#include "net/network_session.hpp"

#include <algorithm>

namespace redgem::net {

using namespace wire;

void NetworkSession::receive_newcomers(float seconds) {
  if (!listener_.valid()) return;
  accept_newcomers();
  drop_silent_newcomers(seconds);
}

void NetworkSession::accept_newcomers() {
  for (Socket guest; listener_.accept(guest) == IoStatus::Ok;) {
    // (Too many waiting already: the newcomer is hung up on.)
    if (!room_to_wait(guest.peer_address())) continue;
    // Let in when it says hello (see admit), if the game has room.
    links_.push_back({Connection::accepted(std::move(guest)), 0});
  }
}

bool NetworkSession::room_to_wait(const std::string& address) const {
  int waiting = 0;
  int from_there = 0;
  for (const Link& link : links_) {
    if (!link.waiting()) continue;
    ++waiting;
    if (link.connection.address() == address) ++from_there;
  }
  return waiting < kMaxWaitingLinks && from_there < waiting_per_address_;
}

void NetworkSession::drop_silent_newcomers(float seconds) {
  for (Link& link : links_) {
    if (link.before_hello()) link.connection.keep_waiting(seconds);
  }
}

void NetworkSession::handle_as_host(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  if (link.asking()) return;  // nothing it says counts while the game decides on it
  if (!link.let_in()) {
    hear_newcomer(link, message, events);
    return;
  }
  if (!is_game_message(message)) return;
  link.idle_for = 0.0f;
  // Who it is from is what the host knows, not what the message claims.
  const nlohmann::json passed = passed_on(message, link.peer);
  if (pass_on(link.peer, passed.value(kTo, kEveryone), passed)) hear_from_game(passed, events);
}

void NetworkSession::hear_newcomer(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  // A newcomer's first words must be a hello; anything else and it is not let in.
  if (message[kKind] != kHello) {
    link.connection.close();
    return;
  }
  if (link.connection.refused_for_protocol(message)) return;
  consider(link, message.value(kAuth, nlohmann::json()), events);
}

void NetworkSession::admit(Link& link, std::vector<NetworkEvent>& events) {
  let_in(link, free_peer_id(ids_taken()), events);
}

void NetworkSession::let_in(Link& link, int peer, std::vector<NetworkEvent>& events) {
  link.peer = peer;
  link.asking_as = 0;
  if (!link.let_in()) {
    link.connection.close();  // full
    return;
  }
  std::vector<int> already_here{kHostId};
  already_here.insert(already_here.end(), peers_.begin(), peers_.end());
  link.connection.send(welcome_for(link.peer, already_here));
  queue_for_everyone_but(link.peer, {{kKind, kArrived}, {kFrom, link.peer}});
  poses_.sent.start_over();  // the newcomer knows none of this machine's objects
  peers_.push_back(link.peer);
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::PeerJoined, link.peer));
}

nlohmann::json NetworkSession::welcome_for(int peer, const std::vector<int>& already_here) const {
  nlohmann::json welcome = {{kKind, kWelcome}, {kTo, peer}, {kPeers, already_here}, {kVersion, kProtocolVersion}};
  if (!server_hosted_) return welcome;
  welcome[kRoom] = room_;
  welcome[kServer] = true;
  return welcome;
}

bool NetworkSession::pass_on(int from, int to, const nlohmann::json& message) {
  if (to == kEveryone) queue_for_everyone_but(from, message);
  if (reaches_host(to)) return true;
  if (Link* other = link_to(to)) other->connection.send(message);
  return false;
}

void NetworkSession::farewell_closed_links(std::vector<NetworkEvent>& events) {
  for (const int peer : take_closed_links()) {
    if (peer == 0) continue;  // never let in: nobody heard of it
    queue_for_everyone_but(peer, {{kKind, kGone}, {kFrom, peer}});
    peer_left(peer, events);
  }
}

std::vector<int> NetworkSession::take_closed_links() {
  std::vector<int> gone;
  for (const Link& link : links_) {
    if (link.connection.closed()) gone.push_back(link.peer);
  }
  links_.erase(std::remove_if(links_.begin(), links_.end(), [](const Link& link) { return link.connection.closed(); }),
               links_.end());
  return gone;
}

bool NetworkSession::send_as_host(const nlohmann::json& message, int to) {
  if (to == kEveryone) {
    queue_for_everyone_but(id_, message);
    return true;
  }
  Link* link = link_to(to);
  if (link == nullptr) return false;
  link->connection.send(message);
  return true;
}

void NetworkSession::queue_for_everyone_but(int peer, const nlohmann::json& message) {
  const std::vector<std::uint8_t> bytes = packed(message);  // once, however many it goes to
  for (Link& link : links_) {
    if (link.is_another_than(peer)) link.connection.send_packed(bytes);
  }
}

NetworkSession::Link* NetworkSession::link_to(int peer) {
  const auto found = std::find_if(links_.begin(), links_.end(), [peer](const Link& link) { return link.peer == peer; });
  return found != links_.end() ? &*found : nullptr;
}

}  // namespace redgem::net
