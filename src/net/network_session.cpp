#include "net/network_session.hpp"

#include "net/wire.hpp"

#include <algorithm>

namespace redgem::net {

namespace {

using namespace wire;

NetworkEvent event_of(NetworkEvent::Kind kind, int peer = 0) {
  NetworkEvent event;
  event.kind = kind;
  event.peer = peer;
  return event;
}

}  // namespace

bool NetworkSession::host(std::uint16_t port, std::string& error) {
  leave();
  listener_ = Socket::listen_on(port, error);
  if (!listener_.valid()) return false;
  port_ = listener_.local_port();
  id_ = kHostId;
  state_ = State::Online;
  announce_hosting_ = true;
  return true;
}

bool NetworkSession::connect_out(const std::string& address, std::uint16_t port, const nlohmann::json& hello,
                                 std::string& error) {
  leave();
  Link link;
  link.socket = Socket::connect_to(address, port, error);
  if (!link.socket.valid()) return false;
  link.peer = kHostId;
  links_.push_back(std::move(link));
  queue(links_.front(), hello);  // goes out once the connection is made
  port_ = port;
  connecting_for_ = 0.0f;
  state_ = State::Connecting;
  return true;
}

bool NetworkSession::join(const std::string& address, std::uint16_t port, std::string& error) {
  return connect_out(address, port, {{kKind, kHello}}, error);
}

bool NetworkSession::host_room(const std::string& address, std::uint16_t port, std::string& error) {
  if (!connect_out(address, port, {{kKind, kHello}, {kRoom, kNewRoom}}, error)) return false;
  through_relay_ = true;
  opening_room_ = true;
  return true;
}

bool NetworkSession::join_room(const std::string& address, std::uint16_t port, const std::string& code,
                               std::string& error) {
  if (code.empty() || code == kNewRoom) {
    error = "a room's code is needed to join it";
    return false;
  }
  if (!connect_out(address, port, {{kKind, kHello}, {kRoom, code}}, error)) return false;
  through_relay_ = true;
  return true;
}

void NetworkSession::leave() {
  // A last try to deliver what was already said.
  for (Link& link : links_) link.socket.write(link.outgoing);
  go_offline();
}

void NetworkSession::go_offline() {
  links_.clear();
  listener_.close();
  peers_.clear();
  state_ = State::Offline;
  id_ = 0;
  port_ = 0;
  announce_hosting_ = false;
  through_relay_ = false;
  opening_room_ = false;
  room_.clear();
}

std::vector<NetworkEvent> NetworkSession::poll(float seconds) {
  std::vector<NetworkEvent> events;
  if (state_ == State::Offline) return events;
  if (announce_hosting_) {
    announce_hosting_ = false;
    events.push_back(event_of(NetworkEvent::Kind::Connected, id_));
  }
  if (listener_.valid()) {
    accept_newcomers();
    drop_silent_newcomers(seconds);
  }
  if (state_ == State::Connecting) await_connection(seconds, events);
  // By index: handling a message may add nothing to links_, but keep it obvious.
  for (std::size_t i = 0; i < links_.size() && state_ != State::Offline; ++i) {
    Link& link = links_[i];
    pump(link);
    if (state_ == State::Online && link.peer != 0) keep_in_touch(link, seconds);
    while (const auto text = link.incoming.next()) {
      handle(link, *text, events);
      if (state_ == State::Offline) break;
    }
    if (link.incoming.broken()) link.closed = true;
  }
  if (state_ != State::Offline) drop_closed_links(events);
  return events;
}

void NetworkSession::accept_newcomers() {
  for (Socket guest = listener_.accept(); guest.valid(); guest = listener_.accept()) {
    if (static_cast<int>(links_.size()) + 1 >= kMaxPeers) continue;  // full: the newcomer is hung up on
    Link link;
    link.socket = std::move(guest);
    links_.push_back(std::move(link));  // let in when it says hello (see admit)
  }
}

void NetworkSession::admit(Link& link, std::vector<NetworkEvent>& events) {
  link.peer = free_peer_id(peers_);
  if (link.peer == 0) {
    link.closed = true;  // full
    return;
  }
  std::vector<int> already_here{kHostId};
  already_here.insert(already_here.end(), peers_.begin(), peers_.end());
  queue(link, {{kKind, kWelcome}, {kTo, link.peer}, {kPeers, already_here}});
  queue_for_everyone_but(link.peer, {{kKind, kArrived}, {kFrom, link.peer}});
  peers_.push_back(link.peer);
  events.push_back(event_of(NetworkEvent::Kind::PeerJoined, link.peer));
}

void NetworkSession::drop_silent_newcomers(float seconds) {
  for (Link& link : links_) {
    if (link.peer != 0) continue;
    link.waiting_for += seconds;
    if (link.waiting_for > kJoinTimeoutSeconds) link.closed = true;  // connected and said nothing
  }
}

void NetworkSession::await_connection(float seconds, std::vector<NetworkEvent>& events) {
  connecting_for_ += seconds;
  const IoStatus status = links_.front().socket.connect_status();
  // Connected to the host: the game starts when its welcome arrives (handle_as_guest).
  if (status == IoStatus::Closed || connecting_for_ > kJoinTimeoutSeconds) {
    go_offline();
    events.push_back(event_of(NetworkEvent::Kind::Failed));
  }
}

void NetworkSession::pump(Link& link) {
  if (state_ == State::Connecting && link.socket.connect_status() != IoStatus::Ok) return;
  std::vector<std::uint8_t> arrived;
  if (link.socket.read(arrived, kMaxReadBytesPerPoll) == IoStatus::Closed) link.closed = true;
  if (!arrived.empty()) link.quiet_for = 0.0f;
  link.incoming.append(arrived.data(), arrived.size());
  if (!link.closed && link.socket.write(link.outgoing) == IoStatus::Closed) link.closed = true;
}

void NetworkSession::keep_in_touch(Link& link, float seconds) {
  link.quiet_for += seconds;
  link.since_said += seconds;
  if (link.quiet_for > kSilenceTimeoutSeconds) {
    link.closed = true;  // gone without a goodbye
  } else if (link.since_said >= kPingEverySeconds) {
    queue(link, {{kKind, kPing}});
  }
}

void NetworkSession::handle(Link& link, const std::string& text, std::vector<NetworkEvent>& events) {
  const nlohmann::json message = nlohmann::json::parse(text, nullptr, false);
  if (!well_formed(message)) {
    link.closed = true;  // not one of ours
    return;
  }
  try {
    if (relaying()) {
      handle_as_host(link, message, events);
    } else {
      handle_as_guest(message, events);
    }
  } catch (const nlohmann::json::exception&) {
    link.closed = true;  // (well_formed should have caught it: a peer never takes the game down)
  }
}

void NetworkSession::handle_as_host(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  if (link.peer == 0) {
    // A newcomer's first words must be a hello; anything else and it is not let in.
    if (message[kKind] == kHello) {
      admit(link, events);
    } else {
      link.closed = true;
    }
    return;
  }
  if (message[kKind] != kGameMessage || !message.contains(kName)) return;
  const int to = message.value(kTo, kEveryone);
  // Who it is from is what the host knows, not what the message claims.
  const nlohmann::json passed_on =
      game_message(message[kName], message.value(kData, nlohmann::json()), link.peer, to);
  if (to == kEveryone) {
    queue_for_everyone_but(link.peer, passed_on);
  } else if (to != kHostId) {
    if (Link* other = link_to(to)) queue(*other, passed_on);
    return;
  }
  NetworkEvent event = event_of(NetworkEvent::Kind::Message, link.peer);
  event.name = passed_on[kName];
  event.data = passed_on[kData];
  events.push_back(std::move(event));
}

void NetworkSession::handle_as_guest(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  const std::string kind = message[kKind];
  if (kind == kWelcome && state_ == State::Connecting) {
    // Only the host is 1 (as is whoever opened a room on a relay), and nobody is 0.
    const int lowest = through_relay_ && opening_room_ ? kHostId : kFirstGuestId;
    const int given = message.value(kTo, 0);
    if (given < lowest || (opening_room_ && given != kHostId)) {
      links_.front().closed = true;
      return;
    }
    id_ = given;
    room_ = message.value(kRoom, std::string());
    peers_ = message.value(kPeers, std::vector<int>{});
    state_ = State::Online;
    events.push_back(event_of(NetworkEvent::Kind::Connected, id_));
  } else if (state_ != State::Online) {
    return;  // nothing counts before the welcome
  } else if (kind == kArrived) {
    const int peer = message.value(kFrom, 0);
    peers_.push_back(peer);
    events.push_back(event_of(NetworkEvent::Kind::PeerJoined, peer));
  } else if (kind == kGone) {
    const int peer = message.value(kFrom, 0);
    peers_.erase(std::remove(peers_.begin(), peers_.end(), peer), peers_.end());
    events.push_back(event_of(NetworkEvent::Kind::PeerLeft, peer));
  } else if (kind == kGameMessage && message.contains(kName)) {
    NetworkEvent event = event_of(NetworkEvent::Kind::Message, message.value(kFrom, 0));
    event.name = message[kName];
    event.data = message.value(kData, nlohmann::json());
    events.push_back(std::move(event));
  }
}

void NetworkSession::drop_closed_links(std::vector<NetworkEvent>& events) {
  if (!relaying()) {
    if (links_.empty() || !links_.front().closed) return;
    // The host is gone, and the game with it.
    const bool was_in = state_ == State::Online;
    go_offline();
    events.push_back(event_of(was_in ? NetworkEvent::Kind::Disconnected : NetworkEvent::Kind::Failed));
    return;
  }
  std::vector<int> gone;
  for (const Link& link : links_) {
    if (link.closed) gone.push_back(link.peer);
  }
  links_.erase(std::remove_if(links_.begin(), links_.end(), [](const Link& link) { return link.closed; }), links_.end());
  for (const int peer : gone) {
    if (peer == 0) continue;  // never let in: nobody heard of it
    peers_.erase(std::remove(peers_.begin(), peers_.end(), peer), peers_.end());
    queue_for_everyone_but(peer, {{kKind, kGone}, {kFrom, peer}});
    events.push_back(event_of(NetworkEvent::Kind::PeerLeft, peer));
  }
}

bool NetworkSession::send(const std::string& name, const nlohmann::json& data, int to) {
  if (state_ != State::Online || to == id_ || name.size() > kMaxNameBytes || !acceptable_data(data)) return false;
  const nlohmann::json message = game_message(name, data, id_, to);
  if (!relaying()) {
    // Everything goes to the host (or the relay), which passes it on.
    if (to != kEveryone && to != kHostId && std::find(peers_.begin(), peers_.end(), to) == peers_.end()) return false;
    queue(links_.front(), message);
    return true;
  }
  if (to == kEveryone) {
    queue_for_everyone_but(id_, message);
    return true;
  }
  Link* link = link_to(to);
  if (link == nullptr) return false;
  queue(*link, message);
  return true;
}

void NetworkSession::queue(Link& link, const nlohmann::json& message) {
  // (Text that is not valid UTF-8 travels with the broken parts replaced, instead of not at all.)
  const std::vector<std::uint8_t> bytes = packed(message);
  link.outgoing.insert(link.outgoing.end(), bytes.begin(), bytes.end());
  link.since_said = 0.0f;
  if (link.outgoing.size() > kMaxPendingBytes) {
    link.closed = true;  // it stopped listening: the game does not pile messages up for it forever
    link.outgoing.clear();
    return;
  }
  // Sent right away when the connection takes it; what is left goes out with the next poll.
  if (state_ == State::Online && link.socket.write(link.outgoing) == IoStatus::Closed) link.closed = true;
}

void NetworkSession::queue_for_everyone_but(int peer, const nlohmann::json& message) {
  for (Link& link : links_) {
    if (link.peer != peer && link.peer != 0) queue(link, message);
  }
}

NetworkSession::Link* NetworkSession::link_to(int peer) {
  const auto found = std::find_if(links_.begin(), links_.end(), [peer](const Link& link) { return link.peer == peer; });
  return found != links_.end() ? &*found : nullptr;
}

}  // namespace redgem::net
