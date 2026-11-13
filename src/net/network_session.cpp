// NetworkSession: getting into a game and out of it, and the turn every poll gives each machine.

#include "net/network_session.hpp"

#include <algorithm>

namespace redgem::net {

using namespace wire;

namespace {

// Whether a text can be the code of a room to join (the word that opens a new one is no room's).
bool names_a_room(const std::string& code) { return !code.empty() && code != kNewRoom; }

// A newcomer's first words, with what the game joined decides on when there is any.
nlohmann::json hello_with(const nlohmann::json& auth) {
  nlohmann::json said = hello();
  if (!auth.is_null()) said[kAuth] = auth;
  return said;
}

}  // namespace

NetworkEvent NetworkEvent::of(Kind kind, int peer) {
  NetworkEvent event;
  event.kind = kind;
  event.peer = peer;
  return event;
}

NetworkEvent NetworkEvent::approval(int peer, int ticket, const nlohmann::json& auth) {
  NetworkEvent event = of(Kind::Approval, peer);
  event.ticket = ticket;
  event.data = auth;
  return event;
}

NetworkEvent NetworkEvent::message(int from, const std::string& name, const nlohmann::json& data) {
  NetworkEvent event = of(Kind::Message, from);
  event.name = name;
  event.data = data;
  return event;
}

bool NetworkSession::host(std::uint16_t port, std::string& error, int waiting_per_address,
                          std::shared_ptr<const TlsIdentity> identity) {
  leave();
  waiting_per_address_ = waiting_per_address;
  listener_ = Socket::listen_on(port, error);
  if (!listener_.valid()) return false;
  port_ = listener_.local_port();
  identity_ = std::move(identity);
  start_hosting();
  return true;
}

std::string NetworkSession::fingerprint() const {
  return identity_ != nullptr ? identity_->fingerprint() : std::string();
}

void NetworkSession::start_hosting() {
  id_ = kHostId;
  state_ = State::Online;
  pending_events_.push_back(NetworkEvent::of(NetworkEvent::Kind::Connected, id_));
}

bool NetworkSession::connect_out(const std::string& address, std::uint16_t port, const nlohmann::json& hello,
                                 const TlsTrust& trust, std::string& error) {
  leave();
  Socket socket = Socket::connect_to(address, port, error);
  if (!socket.valid()) return false;
  links_.push_back({Connection::calling(std::move(socket), trust), kHostId});
  links_.front().connection.send(hello);  // goes out once the connection is made
  rejoin_.address = address;
  rejoin_.trust = trust;
  port_ = port;
  connecting_for_ = 0.0f;
  state_ = State::Connecting;
  return true;
}

bool NetworkSession::join(const std::string& address, std::uint16_t port, std::string& error,
                          const nlohmann::json& auth, const TlsTrust& trust) {
  return connect_out(address, port, hello_with(auth), trust, error);
}

bool NetworkSession::host_room(const std::string& address, std::uint16_t port, std::string& error,
                               const TlsTrust& trust) {
  nlohmann::json said = hello();
  said[kRoom] = kNewRoom;
  said[kGuards] = approving_;
  if (!connect_out(address, port, said, trust, error)) return false;
  through_relay_ = true;
  opening_room_ = true;
  return true;
}

bool NetworkSession::join_room(const std::string& address, std::uint16_t port, const std::string& code,
                               std::string& error, const nlohmann::json& auth, const TlsTrust& trust) {
  if (!names_a_room(code)) {
    error = "a room's code is needed to join it";
    return false;
  }
  nlohmann::json said = hello_with(auth);
  said[kRoom] = code;
  if (!connect_out(address, port, said, trust, error)) return false;
  through_relay_ = true;
  return true;
}

void NetworkSession::leave() {
  if (in_game()) say_bye();
  // A last try to deliver what was already said.
  for (Link& link : links_) link.connection.flush();
  go_offline();
  identity_.reset();  // (only a game hosted here has one, and only leaving ends it)
}

void NetworkSession::go_offline() {
  links_.clear();
  listener_.close();
  peers_.clear();
  room_asking_.clear();
  state_ = State::Offline;
  id_ = 0;
  port_ = 0;
  through_relay_ = false;
  opening_room_ = false;
  server_hosted_ = false;
  room_.clear();
  refusal_.clear();
  pending_events_.clear();
  poses_ = PoseBaselines();
  held_.clear();
  rejoin_ = Rejoin();
}

void NetworkSession::say_bye() {
  // (A relay hears no goodbye: it tells the others itself.)
  if (on_relay()) return;
  if (relaying()) {
    queue_for_everyone_but(id_, word(kBye));
  } else if (!links_.empty()) {
    links_.front().connection.send(word(kBye));
  }
}

std::vector<NetworkEvent> NetworkSession::poll(float seconds) {
  std::vector<NetworkEvent> events;
  if (offline()) return events;
  events.swap(pending_events_);
  tend_newcomers(seconds);
  on_the_way(seconds, events);
  // By index: handling a message may add nothing to links_, but keep it obvious. (And any of
  // them may end the game: nobody after that gets a turn.)
  for (std::size_t i = 0; still_serving(i); ++i) serve(links_[i], seconds, events);
  after_serving(seconds, events);
  return events;
}

void NetworkSession::on_the_way(float seconds, std::vector<NetworkEvent>& events) {
  if (state_ == State::Connecting) await_connection(seconds, events);
  if (state_ == State::Reconnecting) keep_reconnecting(seconds, events);
}

void NetworkSession::after_serving(float seconds, std::vector<NetworkEvent>& events) {
  if (offline()) return;
  drop_closed_links(events);
  if (relaying()) expire_held_places(seconds, events);
}

bool NetworkSession::still_serving(std::size_t link) const { return link < links_.size() && !offline(); }

void NetworkSession::serve(Link& link, float seconds, std::vector<NetworkEvent>& events) {
  police(link, link.connection.pump(), seconds);
  if (in_touch_with(link)) link.connection.keep_in_touch(seconds);
  // (Only so many a poll: the rest wait for the next one, see kMaxMessagesPerPoll.)
  while (!offline()) {
    const auto message = link.connection.next_message();
    if (!message) break;
    if (!police_message(link)) break;  // the message past what the machine may send goes nowhere
    handle(link, *message, events);
  }
}

bool NetworkSession::in_touch_with(const Link& link) const { return state_ == State::Online && link.let_in(); }

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

bool NetworkSession::knows(int peer) const { return std::find(peers_.begin(), peers_.end(), peer) != peers_.end(); }

void NetworkSession::peer_left(int peer, std::vector<NetworkEvent>& events) {
  const auto gone = std::remove(peers_.begin(), peers_.end(), peer);
  if (gone == peers_.end()) return;  // nobody the game heard of
  peers_.erase(gone, peers_.end());
  poses_.heard.forget(peer);
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::PeerLeft, peer));
}

void NetworkSession::drop_closed_links(std::vector<NetworkEvent>& events) {
  if (relaying()) {
    farewell_closed_links(events);
  } else {
    end_game_if_host_gone(events);
  }
}

bool NetworkSession::send(const std::string& name, const nlohmann::json& data, int to) {
  if (!can_send(name, data, to)) return false;
  return deliver(game_message(name, data, id_, to), to);
}

bool NetworkSession::deliver(const nlohmann::json& message, int to) {
  return relaying() ? send_as_host(message, to) : send_through_host(message, to);
}

bool NetworkSession::can_send(const std::string& name, const nlohmann::json& data, int to) const {
  return state_ == State::Online && to != id_ && name.size() <= kMaxNameBytes && acceptable_data(data);
}

}  // namespace redgem::net
