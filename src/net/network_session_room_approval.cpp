// NetworkSession: connection approval in a room this machine opened on a relay. The relay decides
// nothing: it is told whether this game decides who comes in, hands over what each newcomer's
// join carried, and passes the decision back. Meanwhile the newcomer waits at the relay.

#include <algorithm>

#include "net/network_session.hpp"

namespace redgem::net {

using namespace wire;

void NetworkSession::require_approval(bool required) {
  approving_ = required;
  if (guards_a_room()) tell_relay_who_decides();
}

void NetworkSession::tell_relay_who_decides() {
  links_.front().connection.send({{kKind, kGuarded}, {kData, approving_}});
}

void NetworkSession::hear_asking(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  if (!opening_room_) return;  // only the machine that opened a room is asked
  const int peer = message.value(kFrom, 0);
  if (!new_asker(peer)) return;  // what the relay says is checked like anything else
  const nlohmann::json auth = message.value(kAuth, nlohmann::json());
  if (!acceptable_auth(auth)) return send_verdict(peer, false, kUnreadableAuth);
  if (!approving_) return send_verdict(peer, true);
  room_asking_.push_back({peer, ++last_ticket_});
  events.push_back(NetworkEvent::approval(peer, last_ticket_, auth.is_null() ? nlohmann::json::object() : auth));
}

bool NetworkSession::new_asker(int peer) const {
  // (Each id once: there are never more askers than places in a game.)
  const bool asked_already = std::any_of(room_asking_.begin(), room_asking_.end(),
                                         [peer](const RoomAsking& asking) { return asking.peer == peer; });
  return can_arrive(peer) && !asked_already;
}

bool NetworkSession::decide_in_room(int ticket, bool accepted, const std::string& reason) {
  const auto found = std::find_if(room_asking_.begin(), room_asking_.end(),
                                  [ticket](const RoomAsking& asking) { return asking.ticket == ticket; });
  if (found == room_asking_.end()) return false;
  send_verdict(found->peer, accepted, reason);
  room_asking_.erase(found);
  return true;
}

void NetworkSession::send_verdict(int peer, bool accepted, const std::string& reason) {
  links_.front().connection.send(
      {{kKind, kVerdict}, {kTo, peer}, {kData, accepted}, {kReason, reason.substr(0, kMaxReasonBytes)}});
}

void NetworkSession::wait_for_room_decisions(float seconds) {
  for (auto asking = room_asking_.begin(); asking != room_asking_.end();) {
    asking->asking_for += seconds;
    if (asking->asking_for <= kApprovalSeconds) {
      ++asking;
      continue;
    }
    send_verdict(asking->peer, false, kTooLate);
    asking = room_asking_.erase(asking);
  }
}

void NetworkSession::peer_gone(int peer, std::vector<NetworkEvent>& events) {
  room_asking_.erase(std::remove_if(room_asking_.begin(), room_asking_.end(),
                                    [peer](const RoomAsking& asking) { return asking.peer == peer; }),
                     room_asking_.end());
  peer_left(peer, events);
}

}  // namespace redgem::net
