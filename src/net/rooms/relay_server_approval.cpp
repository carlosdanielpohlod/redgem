// RelayServer: connection approval in its rooms. The relay decides nothing: the opener says
// whether its game decides who comes in; when it does, a newcomer is held with the id it would
// have, what its join carried goes to the opener, and the opener's decision comes back. A game that
// never answers is not waited on for ever (kRelayApprovalSeconds).

#include <algorithm>

#include "net/rooms/relay_server.hpp"

namespace redgem::net {

using namespace wire;

namespace {

// What an opener's word on its own room or on a newcomer says: yes only when it says so.
bool says_yes(const nlohmann::json& message) {
  const nlohmann::json said = message.value(kData, nlohmann::json());
  return said.is_boolean() && said.get<bool>();
}

}  // namespace

void RelayServer::ask(Member& member, Member& opener, int id, const nlohmann::json& auth) {
  member.asking_as = id;
  member.connection.send({{kKind, kConsidering}});
  nlohmann::json asked = {{kKind, kAsking}, {kFrom, id}};
  if (!auth.is_null()) asked[kAuth] = auth;
  opener.connection.send(asked);
}

void RelayServer::hear_opener(Member& opener, const nlohmann::json& message) {
  const std::string kind = message[kKind];
  if (kind == kGuarded) opener.guarded = says_yes(message);
  if (kind == kVerdict) settle(opener.room, message);
}

void RelayServer::settle(const std::string& room, const nlohmann::json& verdict) {
  Member* newcomer = asking_member(room, verdict.value(kTo, 0));
  if (newcomer == nullptr) return;  // gone, or decided on already
  if (says_yes(verdict)) {
    let_in(*newcomer, newcomer->asking_as);
  } else {
    newcomer->connection.refuse(verdict.value(kReason, std::string()));  // (the opener hears it is gone)
  }
}

void RelayServer::await_verdict(Member& member, float seconds) {
  member.asking_for += seconds;
  if (member.asking_for > kRelayApprovalSeconds) member.connection.refuse(kTooLate);
}

RelayServer::Member* RelayServer::asking_member(const std::string& room, int id) {
  const auto found = std::find_if(members_.begin(), members_.end(),
                                  [&room, id](const auto& member) { return member->asks_as(room, id); });
  return found != members_.end() ? found->get() : nullptr;
}

RelayServer::Member* RelayServer::opener_of(const std::string& room) {
  for (Member* member : members_of(room)) {
    if (member->holds_room()) return member;
  }
  return nullptr;
}

std::vector<int> RelayServer::seats_taken(const std::string& room) const {
  std::vector<int> taken;
  for (const auto& member : members_) {
    if (member->holds_seat_in(room)) taken.push_back(member->seat());
  }
  return taken;
}

}  // namespace redgem::net
