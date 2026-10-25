// NetworkSession: getting into a game and out of it, and the turn every poll gives each machine.

#include "net/network_session.hpp"
#include <cstdio>

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

bool NetworkSession::host(std::uint16_t port, std::string& error, int waiting_per_address) {
  leave();
  waiting_per_address_ = waiting_per_address;
  listener_ = Socket::listen_on(port, error);
  if (!listener_.valid()) return false;
  port_ = listener_.local_port();
  start_hosting();
  return true;
}

void NetworkSession::start_hosting() {
  id_ = kHostId;
  state_ = State::Online;
  pending_events_.push_back(NetworkEvent::of(NetworkEvent::Kind::Connected, id_));
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

bool NetworkSession::join(const std::string& address, std::uint16_t port, std::string& error,
                          const nlohmann::json& auth) {
  return connect_out(address, port, hello_with(auth), error);
}

bool NetworkSession::host_room(const std::string& address, std::uint16_t port, std::string& error) {
  nlohmann::json said = hello();
  said[kRoom] = kNewRoom;
  said[kGuards] = approving_;
  if (!connect_out(address, port, said, error)) return false;
  through_relay_ = true;
  opening_room_ = true;
  return true;
}

bool NetworkSession::join_room(const std::string& address, std::uint16_t port, const std::string& code,
                               std::string& error, const nlohmann::json& auth) {
  if (!names_a_room(code)) {
    error = "a room's code is needed to join it";
    return false;
  }
  nlohmann::json said = hello_with(auth);
  said[kRoom] = code;
  if (!connect_out(address, port, said, error)) return false;
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
}

std::vector<NetworkEvent> NetworkSession::poll(float seconds) {
  std::vector<NetworkEvent> events;
  if (offline()) return events;
  events.swap(pending_events_);
  tend_newcomers(seconds);
  if (state_ == State::Connecting) await_connection(seconds, events);
  // By index: handling a message may add nothing to links_, but keep it obvious. (And any of
  // them may end the game: nobody after that gets a turn.)
  for (std::size_t i = 0; still_serving(i); ++i) serve(links_[i], seconds, events);
  if (!offline()) drop_closed_links(events);
  return events;
}

bool NetworkSession::still_serving(std::size_t link) const { return link < links_.size() && !offline(); }

void NetworkSession::serve(Link& link, float seconds, std::vector<NetworkEvent>& events) {
  const std::size_t dbg_bytes = link.connection.pump();
  police(link, dbg_bytes, seconds);
  int dbg_taken = 0;
  if (in_touch_with(link)) link.connection.keep_in_touch(seconds);
  // (Only so many a poll: the rest wait for the next one, see kMaxMessagesPerPoll.)
  while (!offline()) {
    const auto message = link.connection.next_message();
    if (!message) break;
    if (!police_message(link)) break;  // the message past what the machine may send goes nowhere
    ++dbg_taken;
    handle(link, *message, events);
  }
  if (relaying()) {
    static int dbg_empty = 0;
    if (dbg_bytes == 0 && dbg_taken == 0) { ++dbg_empty; } else {
      std::fprintf(stderr, "DBG poll e=%d s=%.4f b=%zu t=%d held=%zu l=%.0f f=%.0f ml=%.0f mf=%.0f\n", dbg_empty, seconds, dbg_bytes, dbg_taken, link.connection.held(), link.allowance.left(), link.allowance.forgiven(), link.messages.left(), link.messages.forgiven());
      dbg_empty = 0;
    }
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
