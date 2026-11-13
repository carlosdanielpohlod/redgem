// NetworkSession: what the machine hosting a game does for the ones that joined it. It lets them
// in, passes their messages on, and tells the others who came and went.

#include "net/network_session.hpp"

#include <algorithm>
#include <iterator>

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
    links_.push_back({Connection::accepted(std::move(guest), kMaxPendingBytes, identity_), 0});
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
  if (message[kKind] == kBye) return link.connection.close();  // it left: no place is held for it
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
  if (message.contains(kResume)) return resume(link, message[kResume].get<std::string>(), events);
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
  link.token = token_for_newcomer();
  link.connection.send(welcome_for(link, already_here));
  queue_for_everyone_but(link.peer, word_about(kArrived, link.peer));
  poses_.sent.start_over();  // the newcomer knows none of this machine's objects
  peers_.push_back(link.peer);
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::PeerJoined, link.peer));
}

nlohmann::json NetworkSession::welcome_for(const Link& link, const std::vector<int>& already_here) const {
  nlohmann::json welcome = {{kKind, kWelcome}, {kTo, link.peer}, {kPeers, already_here}, {kVersion, kProtocolVersion}};
  if (!link.token.empty()) {
    welcome[kResume] = link.token;
    welcome[kWindow] = reconnect_window_;
  }
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
  for (const Link& link : take_closed_links()) {
    if (!link.let_in()) continue;  // never let in: nobody heard of it
    if (place_held_for(link)) {
      hold_place(link, events);
    } else {
      farewell(link.peer, events);
    }
  }
}

void NetworkSession::farewell(int peer, std::vector<NetworkEvent>& events) {
  queue_for_everyone_but(peer, word_about(kGone, peer));
  for (Held& held : held_) held.departed.push_back(peer);  // (they hear when they are back)
  peer_left(peer, events);
}

std::vector<NetworkSession::Link> NetworkSession::take_closed_links() {
  const auto open_end =
      std::stable_partition(links_.begin(), links_.end(), [](const Link& link) { return !link.connection.closed(); });
  std::vector<Link> gone(std::make_move_iterator(open_end), std::make_move_iterator(links_.end()));
  links_.erase(open_end, links_.end());
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
