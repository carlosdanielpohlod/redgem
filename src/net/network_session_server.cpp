// NetworkSession: a room of a dedicated server (redgem-server). The server hosts the game and
// plays nobody; the machines come to it through the server's lobby, which hands each one to the
// room it asked for.

#include "net/network_session.hpp"

#include <algorithm>

namespace redgem::net {

void NetworkSession::serve_room(const std::string& code) {
  leave();
  room_ = code;
  server_hosted_ = true;
  start_hosting();
}

void NetworkSession::admit_arrival(Connection connection, const nlohmann::json& auth) {
  // (It said hello to the lobby already: it is let in at once, or the game is asked.)
  links_.push_back({std::move(connection), 0});
  consider(links_.back(), auth, pending_events_);
}

std::vector<int> NetworkSession::players() const {
  std::vector<int> playing = peers_;
  if (server_hosted_) playing.erase(std::remove(playing.begin(), playing.end(), kHostId), playing.end());
  return playing;
}

std::vector<std::string> NetworkSession::addresses() const {
  std::vector<std::string> found;
  for (const Link& link : links_) {
    if (!link.connection.closed()) found.push_back(link.connection.address());
  }
  return found;
}

void NetworkSession::police(Link& link, std::size_t bytes, float seconds) {
  if (!relaying()) return;
  link.allowance.refill(seconds);
  link.messages.refill(seconds);
  if (!link.allowance.charge(bytes)) link.connection.close();  // no game talks this much: a flood
  if (!serving()) return;
  link.idle_for += seconds;
  if (link.idle_for > kServedIdleSeconds) link.connection.close();
}

bool NetworkSession::police_message(Link& link) {
  if (!relaying()) return true;
  if (link.messages.charge(1)) return true;
  link.connection.close();
  return false;
}

void NetworkSession::forgive_stall(float seconds) {
  const float forgiven = std::min(seconds, kMostForgivenStallSeconds);
  for (Link& link : links_) {
    link.allowance.forgive(forgiven);
    link.messages.forgive(forgiven);
  }
}

}  // namespace redgem::net
