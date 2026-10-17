// Whether what arrived is a message of ours: every field there of the kind it should be.

#include <algorithm>
#include <cmath>

#include "net/pose_codec.hpp"
#include "net/wire.hpp"

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

// Nil, true or false, a whole number, a number that is one (not infinity, not NaN), or a text: not
// bytes, which a game's data never is.
bool acceptable_scalar(const nlohmann::json& data) {
  if (data.is_number_float()) return std::isfinite(data.get<double>());
  return !data.is_binary();
}

bool acceptable_at(const nlohmann::json& data, int depth) {
  if (!data.is_structured()) return acceptable_scalar(data);
  if (!within_limits(data, depth)) return false;
  return data.is_object() ? acceptable_members(data, depth + 1) : acceptable_elements(data, depth + 1);
}

bool has_kind(const nlohmann::json& message) {
  return message.is_object() && message.contains(wire::kKind) && message[wire::kKind].is_string();
}

// A whole number from `lowest` to `highest`.
bool is_number_between(const nlohmann::json& value, int lowest, int highest) {
  return value.is_number_integer() && value >= lowest && value <= highest;
}

// The fields below may be missing; one that is there must be of its kind.
bool is_id(const nlohmann::json& message, const char* key) {
  return !message.contains(key) || is_number_between(message[key], 0, kLargestId);
}

bool is_version(const nlohmann::json& message) {
  return !message.contains(wire::kVersion) ||
         is_number_between(message[wire::kVersion], 0, wire::kLargestProtocolVersion);
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

bool is_peer_id(const nlohmann::json& entry) { return is_number_between(entry, kHostId, kLargestId); }

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

bool is_pose(const nlohmann::json& data) { return data.is_binary() && data.get_binary().size() <= kMaxPoseBytes; }

// What a message carries: a pose's bytes, or a game's data.
bool carries_what_it_may(const nlohmann::json& message) {
  if (!message.contains(wire::kData)) return true;
  const nlohmann::json& data = message[wire::kData];
  return message[wire::kKind] == wire::kPose ? is_pose(data) : acceptable_data(data);
}

}  // namespace

bool acceptable_data(const nlohmann::json& data) { return acceptable_at(data, 0); }

namespace wire {

bool well_formed(const nlohmann::json& message) {
  return has_kind(message) && is_id(message, kFrom) && is_id(message, kTo) && is_short_text(message, kName) &&
         is_list_of_peers(message, kPeers) && is_short_text(message, kRoom) && is_flag(message, kServer) &&
         is_flag(message, kGuards) && is_text_up_to(message, kReason, kMaxReasonBytes) && is_version(message) &&
         carries_what_it_may(message);
}

bool acceptable_auth(const nlohmann::json& auth) {
  if (auth.is_null()) return true;
  return auth.is_object() && acceptable_data(auth) &&
         auth.dump(kCompact, ' ', false, nlohmann::json::error_handler_t::replace).size() <= kMaxAuthBytes;
}

int protocol_of(const nlohmann::json& message) {
  const nlohmann::json said = message.is_object() ? message.value(kVersion, nlohmann::json()) : nlohmann::json();
  return is_number_between(said, 0, kLargestProtocolVersion) ? said.get<int>() : 0;
}

bool speaks_our_protocol(const nlohmann::json& message) { return protocol_of(message) == kProtocolVersion; }

bool is_game_message(const nlohmann::json& message) {
  const bool named_message = message[kKind] == kGameMessage && message.contains(kName);
  const bool pose = message[kKind] == kPose && message.contains(kData);
  return named_message || pose;
}

}  // namespace wire

}  // namespace redgem::net
