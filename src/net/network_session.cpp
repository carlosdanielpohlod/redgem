#include "net/network_session.hpp"

#include "net/wire.hpp"

#include <algorithm>

namespace redgem::net {

namespace {

using namespace wire;

// Machines that connected to a host and have not said hello yet: how many from one address, and
// how many in all. They hold no place in the game (those are for who was let in), so whoever
// connects and keeps quiet keeps nobody out but the next few from its own address.
constexpr int kMaxWaitingPerAddress = 4;
constexpr int kMaxWaitingLinks = 32;

NetworkEvent event_of(NetworkEvent::Kind kind, int peer = 0) {
  NetworkEvent event;
  event.kind = kind;
  event.peer = peer;
  return event;
}

NetworkEvent message_event(int from, const std::string& name, const nlohmann::json& data) {
  NetworkEvent event = event_of(NetworkEvent::Kind::Message, from);
  event.name = name;
  event.data = data;
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
  Socket socket = Socket::connect_to(address, port, error);
  if (!socket.valid()) return false;
  links_.push_back({Connection::calling(std::move(socket)), kHostId});
  links_.front().connection.send(hello);  // goes out once the connection is made
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
  for (Link& link : links_) link.connection.flush();
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
  announce_hosting(events);
  receive_newcomers(seconds);
  if (state_ == State::Connecting) await_connection(seconds, events);
  // By index: handling a message may add nothing to links_, but keep it obvious.
  for (std::size_t i = 0; i < links_.size() && state_ != State::Offline; ++i) serve(links_[i], seconds, events);
  if (state_ != State::Offline) drop_closed_links(events);
  return events;
}

void NetworkSession::receive_newcomers(float seconds) {
  if (!listener_.valid()) return;
  accept_newcomers();
  drop_silent_newcomers(seconds);
}

void NetworkSession::announce_hosting(std::vector<NetworkEvent>& events) {
  if (!announce_hosting_) return;
  announce_hosting_ = false;
  events.push_back(event_of(NetworkEvent::Kind::Connected, id_));
}

void NetworkSession::serve(Link& link, float seconds, std::vector<NetworkEvent>& events) {
  link.connection.pump();
  if (state_ == State::Online && link.peer != 0) link.connection.keep_in_touch(seconds);
  // (Only so many a poll: the rest wait for the next one, see kMaxMessagesPerPoll.)
  while (state_ != State::Offline) {
    const auto message = link.connection.next_message();
    if (!message) break;
    handle(link, *message, events);
  }
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
    if (link.peer != 0 || link.connection.closed()) continue;
    ++waiting;
    if (link.connection.address() == address) ++from_there;
  }
  return waiting < kMaxWaitingLinks && from_there < kMaxWaitingPerAddress;
}

void NetworkSession::admit(Link& link, std::vector<NetworkEvent>& events) {
  link.peer = free_peer_id(peers_);
  if (link.peer == 0) {
    link.connection.close();  // full
    return;
  }
  std::vector<int> already_here{kHostId};
  already_here.insert(already_here.end(), peers_.begin(), peers_.end());
  link.connection.send({{kKind, kWelcome}, {kTo, link.peer}, {kPeers, already_here}});
  queue_for_everyone_but(link.peer, {{kKind, kArrived}, {kFrom, link.peer}});
  peers_.push_back(link.peer);
  events.push_back(event_of(NetworkEvent::Kind::PeerJoined, link.peer));
}

void NetworkSession::drop_silent_newcomers(float seconds) {
  for (Link& link : links_) {
    if (link.peer == 0) link.connection.keep_waiting(seconds);
  }
}

void NetworkSession::await_connection(float seconds, std::vector<NetworkEvent>& events) {
  // Connected to the host, the game starts when its welcome arrives (accept_welcome); refused, the
  // link closes and the game ends with it (end_game_if_host_gone).
  connecting_for_ += seconds;
  if (connecting_for_ <= kJoinTimeoutSeconds) return;
  go_offline();
  events.push_back(event_of(NetworkEvent::Kind::Failed));
}

void NetworkSession::handle(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  try {
    if (relaying()) {
      handle_as_host(link, message, events);
    } else {
      handle_as_guest(message, events);
    }
  } catch (const nlohmann::json::exception&) {
    link.connection.close();  // (well_formed should have caught it: a peer never takes the game down)
  }
}

void NetworkSession::handle_as_host(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  if (link.peer == 0) {
    hear_newcomer(link, message, events);
    return;
  }
  if (message[kKind] != kGameMessage || !message.contains(kName)) return;
  const int to = message.value(kTo, kEveryone);
  // Who it is from is what the host knows, not what the message claims.
  const nlohmann::json passed_on = game_message(message[kName], message.value(kData, nlohmann::json()), link.peer, to);
  if (pass_on(link.peer, to, passed_on)) events.push_back(message_event(link.peer, passed_on[kName], passed_on[kData]));
}

void NetworkSession::hear_newcomer(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  // A newcomer's first words must be a hello; anything else and it is not let in.
  if (message[kKind] != kHello) {
    link.connection.close();
    return;
  }
  admit(link, events);
}

bool NetworkSession::pass_on(int from, int to, const nlohmann::json& message) {
  if (to == kEveryone) queue_for_everyone_but(from, message);
  if (to == kEveryone || to == kHostId) return true;
  if (Link* other = link_to(to)) other->connection.send(message);
  return false;
}

void NetworkSession::handle_as_guest(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  const std::string kind = message[kKind];
  if (kind == kWelcome && state_ == State::Connecting) {
    accept_welcome(message, events);
    return;
  }
  if (state_ != State::Online) return;  // nothing counts before the welcome
  if (kind == kArrived) peer_arrived(message.value(kFrom, 0), events);
  if (kind == kGone) peer_left(message.value(kFrom, 0), events);
  if (kind == kGameMessage && message.contains(kName)) hear_game_message(message, events);
}

void NetworkSession::hear_game_message(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  events.push_back(message_event(message.value(kFrom, 0), message[kName], message.value(kData, nlohmann::json())));
}

void NetworkSession::accept_welcome(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  const int given = message.value(kTo, 0);
  // (well_formed saw to the list: ids there can be, each once. This machine is not one of the others.)
  const std::vector<int> others = message.value(kPeers, std::vector<int>{});
  const bool listed_itself = std::find(others.begin(), others.end(), given) != others.end();
  if (listed_itself || !own_id_believable(given, through_relay_, opening_room_)) {
    links_.front().connection.close();
    return;
  }
  id_ = given;
  room_ = message.value(kRoom, std::string());
  peers_ = others;
  state_ = State::Online;
  events.push_back(event_of(NetworkEvent::Kind::Connected, id_));
}

void NetworkSession::peer_arrived(int peer, std::vector<NetworkEvent>& events) {
  // What a host says is checked like anything else: nobody arrives twice, as this machine, or as nobody.
  const bool known = std::find(peers_.begin(), peers_.end(), peer) != peers_.end();
  if (known || peer == id_ || peer < kHostId) return;
  peers_.push_back(peer);
  events.push_back(event_of(NetworkEvent::Kind::PeerJoined, peer));
}

void NetworkSession::peer_left(int peer, std::vector<NetworkEvent>& events) {
  const auto gone = std::remove(peers_.begin(), peers_.end(), peer);
  if (gone == peers_.end()) return;  // nobody the game heard of
  peers_.erase(gone, peers_.end());
  events.push_back(event_of(NetworkEvent::Kind::PeerLeft, peer));
}

void NetworkSession::drop_closed_links(std::vector<NetworkEvent>& events) {
  if (!relaying()) {
    end_game_if_host_gone(events);
    return;
  }
  for (const int peer : take_closed_links()) {
    if (peer == 0) continue;  // never let in: nobody heard of it
    queue_for_everyone_but(peer, {{kKind, kGone}, {kFrom, peer}});
    peer_left(peer, events);
  }
}

void NetworkSession::end_game_if_host_gone(std::vector<NetworkEvent>& events) {
  if (links_.empty() || !links_.front().connection.closed()) return;
  // The host is gone, and the game with it.
  const bool was_in = state_ == State::Online;
  go_offline();
  events.push_back(event_of(was_in ? NetworkEvent::Kind::Disconnected : NetworkEvent::Kind::Failed));
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

bool NetworkSession::send(const std::string& name, const nlohmann::json& data, int to) {
  if (state_ != State::Online || to == id_ || name.size() > kMaxNameBytes || !acceptable_data(data)) return false;
  const nlohmann::json message = game_message(name, data, id_, to);
  return relaying() ? send_as_host(message, to) : send_through_host(message, to);
}

bool NetworkSession::send_through_host(const nlohmann::json& message, int to) {
  // Everything goes to the host (or the relay), which passes it on.
  if (to != kEveryone && to != kHostId && std::find(peers_.begin(), peers_.end(), to) == peers_.end()) return false;
  links_.front().connection.send(message);
  return true;
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
  for (Link& link : links_) {
    if (link.peer != peer && link.peer != 0) link.connection.send(message);
  }
}

NetworkSession::Link* NetworkSession::link_to(int peer) {
  const auto found = std::find_if(links_.begin(), links_.end(), [peer](const Link& link) { return link.peer == peer; });
  return found != links_.end() ? &*found : nullptr;
}

}  // namespace redgem::net
