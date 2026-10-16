// The messages machines send each other, as they are made.

#include "net/wire.hpp"

#include <algorithm>

namespace redgem::net::wire {

nlohmann::json game_message(const std::string& name, const nlohmann::json& data, int from, int to) {
  nlohmann::json message = {{kKind, kGameMessage}, {kName, name}, {kFrom, from}, {kData, data}};
  if (to != kEveryone) message[kTo] = to;  // (everyone, when it says nobody: the bytes are saved)
  return message;
}

nlohmann::json passed_on(const nlohmann::json& message, int from) {
  nlohmann::json passed = {{kKind, message[kKind]}, {kFrom, from}, {kData, message.value(kData, nlohmann::json())}};
  if (message.contains(kName)) passed[kName] = message[kName];
  const int to = message.value(kTo, kEveryone);
  if (to != kEveryone) passed[kTo] = to;
  return passed;
}

nlohmann::json refusal(const std::string& reason) {
  return {{kKind, kRefused}, {kReason, reason.substr(0, kMaxReasonBytes)}};
}

bool own_id_believable(int given, bool through_relay, bool opening_room) {
  const bool opens_on_relay = through_relay && opening_room;
  const int lowest = opens_on_relay ? kHostId : kFirstGuestId;
  return given >= lowest && (!opening_room || given == kHostId);
}

int free_peer_id(const std::vector<int>& taken) {
  for (int id = kFirstGuestId; id <= kMaxPeers; ++id) {
    if (std::find(taken.begin(), taken.end(), id) == taken.end()) return id;
  }
  return 0;
}

}  // namespace redgem::net::wire
