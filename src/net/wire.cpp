// The messages machines send each other, as they are made.

#include "net/wire.hpp"

#include <algorithm>
#include <cmath>

namespace redgem::net::wire {

nlohmann::json hello() { return {{kKind, kHello}, {kVersion, kProtocolVersion}}; }

nlohmann::json game_message(const std::string& name, const nlohmann::json& data, int from, int to) {
  nlohmann::json message = {{kKind, kGameMessage}, {kName, name}, {kFrom, from}, {kData, data}};
  if (to != kEveryone) message[kTo] = to;  // (everyone, when it says nobody: the bytes are saved)
  return message;
}

nlohmann::json pose_message(const std::vector<std::uint8_t>& pose, int from) {
  return {{kKind, kPose}, {kFrom, from}, {kData, nlohmann::json::binary(pose)}};
}

bool holds_time(const nlohmann::json& message, const char* key) {
  const auto found = message.find(key);
  return found != message.end() && found->is_number() && std::isfinite(found->get<double>());
}

std::optional<double> time_in(const nlohmann::json& message, const char* key) {
  if (!holds_time(message, key)) return std::nullopt;
  return message[key].get<double>();
}

nlohmann::json stamped(nlohmann::json message, double host_time) {
  message[kHostTime] = host_time;
  return message;
}

nlohmann::json passed_on(const nlohmann::json& message, int from) {
  nlohmann::json passed = {{kKind, message[kKind]}, {kFrom, from}, {kData, message.value(kData, nlohmann::json())}};
  if (message.contains(kName)) passed[kName] = message[kName];
  if (holds_time(message, kViewTime)) passed[kViewTime] = message[kViewTime];
  const int to = message.value(kTo, kEveryone);
  if (to != kEveryone) passed[kTo] = to;
  return passed;
}

nlohmann::json refusal(const std::string& reason) {
  return {{kKind, kRefused}, {kReason, reason.substr(0, kMaxReasonBytes)}, {kVersion, kProtocolVersion}};
}

nlohmann::json refusal_of_protocol(int theirs) { return {{kKind, kRefused}, {kVersion, theirs}}; }

nlohmann::json considering() { return {{kKind, kConsidering}, {kVersion, kProtocolVersion}}; }

nlohmann::json resume_hello(const std::string& token) {
  nlohmann::json said = hello();
  said[kResume] = token;
  return said;
}

nlohmann::json word(const char* kind) { return {{kKind, kind}}; }

nlohmann::json word_about(const char* kind, int peer) { return {{kKind, kind}, {kFrom, peer}}; }

std::string protocol_mismatch(int theirs) {
  return "this game runs protocol " + std::to_string(kProtocolVersion) + ", the other side " + std::to_string(theirs);
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
