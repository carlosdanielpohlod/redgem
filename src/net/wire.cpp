#include "net/wire.hpp"

#include <algorithm>

namespace redgem::net {

namespace {

constexpr int kCompact = -1;  // nlohmann's "no indentation"
constexpr int kLargestId = kMaxPeers;

bool acceptable_at(const nlohmann::json& data, int depth) {
  if (!data.is_structured()) return true;
  if (depth >= kMaxDataDepth) return false;
  if (data.is_object()) {
    for (const auto& [key, value] : data.items()) {
      if (key.size() > kMaxNameBytes || !acceptable_at(value, depth + 1)) return false;
    }
    return true;
  }
  for (const nlohmann::json& value : data) {
    if (!acceptable_at(value, depth + 1)) return false;
  }
  return true;
}

bool is_id(const nlohmann::json& message, const char* key) {
  return !message.contains(key) ||
         (message[key].is_number_integer() && message[key] >= 0 && message[key] <= kLargestId);
}

}  // namespace

bool acceptable_data(const nlohmann::json& data) { return acceptable_at(data, 0); }

namespace wire {

nlohmann::json game_message(const std::string& name, const nlohmann::json& data, int from, int to) {
  return {{kKind, kGameMessage}, {kName, name}, {kFrom, from}, {kTo, to}, {kData, data}};
}

bool well_formed(const nlohmann::json& message) {
  if (!message.is_object() || !message.contains(kKind) || !message[kKind].is_string()) return false;
  if (!is_id(message, kFrom) || !is_id(message, kTo)) return false;
  if (message.contains(kName) && (!message[kName].is_string() || message[kName].get_ref<const std::string&>().size() > kMaxNameBytes)) {
    return false;
  }
  if (message.contains(kPeers)) {
    if (!message[kPeers].is_array()) return false;
    for (const nlohmann::json& peer : message[kPeers]) {
      if (!peer.is_number_integer()) return false;
    }
  }
  if (message.contains(kRoom) && (!message[kRoom].is_string() || message[kRoom].get_ref<const std::string&>().size() > kMaxNameBytes)) {
    return false;
  }
  return !message.contains(kData) || acceptable_data(message[kData]);
}

int free_peer_id(const std::vector<int>& taken) {
  for (int id = kFirstGuestId; id <= kMaxPeers; ++id) {
    if (std::find(taken.begin(), taken.end(), id) == taken.end()) return id;
  }
  return 0;
}

std::vector<std::uint8_t> packed(const nlohmann::json& message) {
  return frame(message.dump(kCompact, ' ', false, nlohmann::json::error_handler_t::replace));
}

}  // namespace wire

}  // namespace redgem::net
