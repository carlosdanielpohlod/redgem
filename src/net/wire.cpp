#include "net/wire.hpp"

#include <algorithm>

namespace redgem::net {

namespace {

constexpr int kCompact = -1;  // nlohmann's "no indentation"
constexpr int kLargestId = kMaxPeers;

bool acceptable_at(const nlohmann::json& data, int depth);

bool acceptable_members(const nlohmann::json& object, int depth) {
  for (const auto& [key, value] : object.items()) {
    if (key.size() > kMaxNameBytes) return false;
    if (!acceptable_at(value, depth)) return false;
  }
  return true;
}

bool acceptable_elements(const nlohmann::json& array, int depth) {
  return std::all_of(array.begin(), array.end(),
                     [depth](const nlohmann::json& value) { return acceptable_at(value, depth); });
}

// Whether an array or a hash found `depth` levels down may be there, and holds no more than one may.
bool within_limits(const nlohmann::json& data, int depth) {
  return depth < kMaxDataDepth && data.size() <= kMaxDataMembers;
}

bool acceptable_at(const nlohmann::json& data, int depth) {
  if (!data.is_structured()) return true;
  if (!within_limits(data, depth)) return false;
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

bool is_text_up_to(const nlohmann::json& message, const char* key, std::size_t most_bytes) {
  return !message.contains(key) ||
         (message[key].is_string() && message[key].get_ref<const std::string&>().size() <= most_bytes);
}

bool is_short_text(const nlohmann::json& message, const char* key) {
  return is_text_up_to(message, key, kMaxNameBytes);
}

bool is_flag(const nlohmann::json& message, const char* key) {
  return !message.contains(key) || message[key].is_boolean();
}

bool is_peer_id(const nlohmann::json& entry) {
  return entry.is_number_integer() && entry >= kHostId && entry <= kLargestId;
}

// A list short enough to be the others of a game, of ids there can be.
bool is_list_of_ids(const nlohmann::json& list) {
  return list.is_array() && list.size() < static_cast<std::size_t>(kMaxPeers) &&
         std::all_of(list.begin(), list.end(), is_peer_id);
}

// Who else is in a game: ids there can be, each once, and never a full game's worth (whoever is
// told is in it too).
bool is_list_of_peers(const nlohmann::json& message, const char* key) {
  if (!message.contains(key)) return true;
  const nlohmann::json& list = message[key];
  if (!is_list_of_ids(list)) return false;
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
         is_list_of_peers(message, kPeers) && is_short_text(message, kRoom) && is_flag(message, kServer) &&
         is_text_up_to(message, kReason, kMaxReasonBytes) &&
         (!message.contains(kData) || acceptable_data(message[kData]));
}

bool acceptable_auth(const nlohmann::json& auth) {
  if (auth.is_null()) return true;
  return auth.is_object() && acceptable_data(auth) &&
         auth.dump(kCompact, ' ', false, nlohmann::json::error_handler_t::replace).size() <= kMaxAuthBytes;
}

bool is_game_message(const nlohmann::json& message) {
  return message[kKind] == kGameMessage && message.contains(kName);
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

std::vector<std::uint8_t> packed(const nlohmann::json& message) {
  return frame(message.dump(kCompact, ' ', false, nlohmann::json::error_handler_t::replace));
}

}  // namespace wire

}  // namespace redgem::net
