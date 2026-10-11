#include "net/wire.hpp"

#include <algorithm>

namespace redgem::net {

namespace {

constexpr int kCompact = -1;  // nlohmann's "no indentation"
constexpr int kLargestId = kMaxPeers;

bool acceptable_at(const nlohmann::json& data, int depth);

bool acceptable_members(const nlohmann::json& object, int depth) {
  for (const auto& [key, value] : object.items()) {
    if (key.size() > kMaxNameBytes || !acceptable_at(value, depth)) return false;
  }
  return true;
}

bool acceptable_elements(const nlohmann::json& array, int depth) {
  return std::all_of(array.begin(), array.end(),
                     [depth](const nlohmann::json& value) { return acceptable_at(value, depth); });
}

bool acceptable_at(const nlohmann::json& data, int depth) {
  if (!data.is_structured()) return true;
  if (depth >= kMaxDataDepth || data.size() > kMaxDataMembers) return false;
  return data.is_object() ? acceptable_members(data, depth + 1) : acceptable_elements(data, depth + 1);
}

bool has_kind(const nlohmann::json& message) {
  return message.is_object() && message.contains(wire::kKind) && message[wire::kKind].is_string();
}

// The fields below may be missing; one that is there must be of its kind.
bool is_id(const nlohmann::json& message, const char* key) {
  return !message.contains(key) ||
         (message[key].is_number_integer() && message[key] >= 0 && message[key] <= kLargestId);
}

bool is_short_text(const nlohmann::json& message, const char* key) {
  return !message.contains(key) ||
         (message[key].is_string() && message[key].get_ref<const std::string&>().size() <= kMaxNameBytes);
}

bool is_peer_id(const nlohmann::json& entry) {
  return entry.is_number_integer() && entry >= kHostId && entry <= kLargestId;
}

// Who else is in a game: ids there can be, each once, and never a full game's worth (whoever is
// told is in it too).
bool is_list_of_peers(const nlohmann::json& message, const char* key) {
  if (!message.contains(key)) return true;
  const nlohmann::json& list = message[key];
  if (!list.is_array() || list.size() >= static_cast<std::size_t>(kMaxPeers)) return false;
  if (!std::all_of(list.begin(), list.end(), is_peer_id)) return false;
  std::vector<int> ids = list.get<std::vector<int>>();
  std::sort(ids.begin(), ids.end());
  return std::adjacent_find(ids.begin(), ids.end()) == ids.end();
}

}  // namespace

bool acceptable_data(const nlohmann::json& data) { return acceptable_at(data, 0); }

namespace wire {

nlohmann::json game_message(const std::string& name, const nlohmann::json& data, int from, int to) {
  return {{kKind, kGameMessage}, {kName, name}, {kFrom, from}, {kTo, to}, {kData, data}};
}

bool well_formed(const nlohmann::json& message) {
  return has_kind(message) && is_id(message, kFrom) && is_id(message, kTo) && is_short_text(message, kName) &&
         is_list_of_peers(message, kPeers) && is_short_text(message, kRoom) &&
         (!message.contains(kData) || acceptable_data(message[kData]));
}

bool own_id_believable(int given, bool through_relay, bool opening_room) {
  const int lowest = through_relay && opening_room ? kHostId : kFirstGuestId;
  return given >= lowest && (!opening_room || given == kHostId);
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
