// NetworkSession: reconnecting, on the machine hosting the game (directly, or a dedicated
// server's room). A machine whose connection dropped keeps its place and id for as long as the
// game says (allow_reconnect); it gets them back by calling again with the token its welcome gave
// it. The others hear it left only when that time is over.

#include <algorithm>
#include <iterator>

#include "net/network_session.hpp"
#include "net/resume_token.hpp"

namespace redgem::net {

using namespace wire;

void NetworkSession::allow_reconnect(float seconds) {
  // (Not a number, or nothing: off.)
  reconnect_window_ = seconds > 0.0f ? std::min(seconds, kMostReconnectWindowSeconds) : 0.0f;
}

std::string NetworkSession::token_for_newcomer() const {
  return reconnect_window_ > 0.0f ? new_resume_token() : std::string();
}

void NetworkSession::hold_place(const Link& link, std::vector<NetworkEvent>& events) {
  held_.push_back({link.peer, link.token, 0.0f});
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::Reconnecting, link.peer));
}

void NetworkSession::expire_held_places(float seconds, std::vector<NetworkEvent>& events) {
  for (Held& held : held_) held.held_for += seconds;
  const auto expired = std::stable_partition(held_.begin(), held_.end(),
                                             [this](const Held& held) { return held.held_for <= reconnect_window_; });
  const std::vector<Held> gone(expired, held_.end());
  held_.erase(expired, held_.end());
  for (const Held& held : gone) farewell(held.peer, events);
}

void NetworkSession::resume(Link& link, const std::string& token, std::vector<NetworkEvent>& events) {
  const Held claimed = claim_place(token);
  if (claimed.peer == 0) return refuse(link, kResumeRefused);
  link.peer = claimed.peer;
  link.token = token_for_newcomer();  // a token gets a place back once
  link.connection.send(welcome_back(link));
  tell_departures(link, claimed.departed);
  // The others start its objects over from whole poses: it missed what they sent meanwhile.
  queue_for_everyone_but(link.peer, word_about(kBack, link.peer));
  hear_back(link.peer);
  events.push_back(NetworkEvent::of(NetworkEvent::Kind::Reconnected, link.peer));
}

void NetworkSession::tell_departures(Link& link, const std::vector<int>& departed) {
  // (Whoever left and is not back under that id is missing from its welcome already: told twice,
  // the second is nobody it knows of.)
  for (const int peer : departed) {
    link.connection.send(word_about(kGone, peer));
    if (knows(peer)) link.connection.send(word_about(kArrived, peer));
  }
}

NetworkSession::Held NetworkSession::claim_place(const std::string& token) {
  if (reconnect_window_ <= 0.0f) return {};
  Held held = claim_held(token);
  return held.peer != 0 ? held : claim_open(token);
}

NetworkSession::Held NetworkSession::claim_held(const std::string& token) {
  const auto found = std::find_if(held_.begin(), held_.end(),
                                  [&token](const Held& held) { return same_resume_token(token, held.token); });
  if (found == held_.end()) return {};
  Held claimed = std::move(*found);
  held_.erase(found);
  return claimed;
}

NetworkSession::Held NetworkSession::claim_open(const std::string& token) {
  // Its old connection is still open here (this side has not noticed the drop yet): it is let go
  // without a goodbye, the place going on through the new one. (A newcomer has no token yet, and
  // an empty token is nobody's.)
  const auto found = std::find_if(links_.begin(), links_.end(),
                                  [&token](const Link& link) { return same_resume_token(token, link.token); });
  if (found == links_.end()) return {};
  Held claimed{found->peer, std::move(found->token)};
  found->peer = 0;
  found->token.clear();
  found->connection.close();
  return claimed;
}

nlohmann::json NetworkSession::welcome_back(const Link& link) const {
  std::vector<int> others{kHostId};
  std::copy_if(peers_.begin(), peers_.end(), std::back_inserter(others),
               [&link](int peer) { return peer != link.peer; });
  return welcome_for(link, others);
}

void NetworkSession::hear_back(int peer) {
  // Whoever reconnected knows none of this machine's objects, and starts its own over too.
  if (!knows(peer)) return;
  poses_.sent.start_over();
  poses_.heard.forget(peer);
}

}  // namespace redgem::net
