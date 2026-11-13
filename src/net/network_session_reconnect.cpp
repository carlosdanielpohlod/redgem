// NetworkSession: reconnecting, on the machine whose connection to its host dropped. When the host
// holds places (its welcome gave a token), this machine stays in the game, with its id and the
// others, and calls the host again every kRedialSeconds until it is let back in, turned away, or
// the time the host holds its place is over.

#include <algorithm>

#include "net/network_session.hpp"

namespace redgem::net {

using namespace wire;

bool NetworkSession::can_reconnect() const {
  return state_ == State::Online && !rejoin_.token.empty() && links_.front().connection.lost();
}

void NetworkSession::start_reconnecting(std::vector<NetworkEvent>& events) {
  links_.clear();
  state_ = State::Reconnecting;
  rejoin_.trying_for = 0.0f;
  rejoin_.redial_in = 0.0f;
  // What the others know of this machine's objects, and it of theirs, starts over once it is back.
  poses_ = PoseBaselines();
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::Reconnecting, id_));
}

void NetworkSession::keep_reconnecting(float seconds, std::vector<NetworkEvent>& events) {
  rejoin_.trying_for += seconds;
  if (rejoin_.trying_for > rejoin_.window) return lose(kReconnectTimedOut, events);
  // A call on its way gets as long as a join to be answered.
  if (!links_.empty()) return links_.front().connection.keep_waiting(seconds);
  rejoin_.redial_in -= seconds;
  if (rejoin_.redial_in <= 0.0f) redial();
}

void NetworkSession::redial() {
  rejoin_.redial_in = kRedialSeconds;
  std::string error;
  Socket socket = Socket::connect_to(rejoin_.address, port_, error);
  if (!socket.valid()) return;  // (tried again in a moment)
  nlohmann::json said = resume_hello(rejoin_.token);
  if (!room_.empty()) said[kRoom] = room_;  // a dedicated server's room
  links_.push_back({Connection::calling(std::move(socket), rejoin_.trust), kHostId});
  links_.front().connection.send(said);
}

void NetworkSession::call_failed(std::vector<NetworkEvent>& events) {
  // Turned away, or another machine than the host answered: there is no getting back in.
  const std::string why = why_host_gone();
  if (!why.empty()) return lose(why, events);
  links_.clear();  // nobody answered: called again in a moment
}

void NetworkSession::hear_while_reconnecting(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  const std::string kind = message[kKind];
  if (kind == kRefused) turned_away(message);
  if (kind == kWelcome) accept_welcome_back(message, events);
}

void NetworkSession::accept_welcome_back(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  const std::vector<int> others = message.value(kPeers, std::vector<int>{});
  if (!believable_welcome_back(message.value(kTo, 0), others)) {
    links_.front().connection.close();
    return;
  }
  keep_resume(message);
  catch_up_with(others, events);
  state_ = State::Online;
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::Reconnected, id_));
}

bool NetworkSession::believable_welcome_back(int given, const std::vector<int>& others) const {
  return given == id_ && believable_welcome(given, others);  // its own place, and nobody else's
}

void NetworkSession::catch_up_with(const std::vector<int>& others, std::vector<NetworkEvent>& events) {
  const auto listed = [&others](int peer) { return std::find(others.begin(), others.end(), peer) != others.end(); };
  const std::vector<int> before = peers_;
  for (const int peer : before) {
    if (!listed(peer)) peer_left(peer, events);
  }
  for (const int peer : others) peer_arrived(peer, events);  // (only those it did not know of)
}

void NetworkSession::lose(const std::string& why, std::vector<NetworkEvent>& events) {
  NetworkEvent lost = NetworkEvent::of(NetworkEvent::Kind::Lost, id_);
  lost.name = why;
  go_offline();
  events.push_back(lost);
}

void NetworkSession::keep_resume(const nlohmann::json& welcome) {
  rejoin_.token = welcome.value(kResume, std::string());
  rejoin_.window = welcome.value(kWindow, 0.0f);
}

}  // namespace redgem::net
