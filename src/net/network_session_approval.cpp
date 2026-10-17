// NetworkSession: connection approval. A newcomer's hello carries what the game decides on (the
// auth: of Network.join); the game hosting is asked, with an Approval event, and has
// kApprovalSeconds to let the newcomer in or turn it away with a reason. Meanwhile the newcomer
// holds the id it would have, and is told to wait.

#include <algorithm>

#include "net/network_session.hpp"

namespace redgem::net {

using namespace wire;

void NetworkSession::consider(Link& link, const nlohmann::json& auth, std::vector<NetworkEvent>& events) {
  if (!acceptable_auth(auth)) return refuse(link, kUnreadableAuth);
  if (approving_) return ask(link, auth, events);
  admit(link, events);
}

void NetworkSession::ask(Link& link, const nlohmann::json& auth, std::vector<NetworkEvent>& events) {
  link.asking_as = free_peer_id(ids_taken());
  if (!link.asking()) {
    link.connection.close();  // no place left
    return;
  }
  link.ticket = ++last_ticket_;
  link.connection.send(considering());
  events.push_back(
      NetworkEvent::approval(link.asking_as, link.ticket, auth.is_null() ? nlohmann::json::object() : auth));
}

bool NetworkSession::decide(int ticket, bool accepted, const std::string& reason) {
  Link* link = asking_link(ticket);
  if (link == nullptr) return decide_in_room(ticket, accepted, reason);
  if (accepted) {
    let_in(*link, link->asking_as, pending_events_);  // the game hears of it with the next poll
  } else {
    refuse(*link, reason);
  }
  return true;
}

void NetworkSession::refuse(Link& link, const std::string& reason) {
  link.asking_as = 0;
  link.connection.refuse(reason);
}

void NetworkSession::tend_newcomers(float seconds) {
  receive_newcomers(seconds);
  if (relaying()) {
    wait_for_decisions(seconds);
  } else {
    wait_for_room_decisions(seconds);
  }
}

void NetworkSession::wait_for_decisions(float seconds) {
  for (Link& link : links_) {
    if (!link.asking()) continue;
    link.asking_for += seconds;
    if (link.asking_for > kApprovalSeconds) refuse(link, kTooLate);
  }
}

NetworkSession::Link* NetworkSession::asking_link(int ticket) {
  const auto found =
      std::find_if(links_.begin(), links_.end(), [ticket](const Link& link) { return link.asks_with(ticket); });
  return found != links_.end() ? &*found : nullptr;
}

std::size_t NetworkSession::deciding() const {
  return room_asking_.size() + static_cast<std::size_t>(std::count_if(links_.begin(), links_.end(),
                                                                      [](const Link& link) { return link.asking(); }));
}

std::vector<int> NetworkSession::ids_taken() const {
  std::vector<int> taken = peers_;
  for (const Link& link : links_) {
    if (link.asking()) taken.push_back(link.asking_as);
  }
  return taken;
}

}  // namespace redgem::net
