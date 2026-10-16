// NetworkSession: what machines in a game hear from each other, and the poses of the objects they
// move (NetworkSync), sent in a format of their own (pose_codec.hpp), saying only what changed.

#include "net/network_session.hpp"

namespace redgem::net {

using namespace wire;

namespace {

nlohmann::json numbers_of(Vec3 vector) { return nlohmann::json::array({vector.x, vector.y, vector.z}); }

// A pose as the game's scripts hear it (kPoseMessageName).
nlohmann::json placement(const std::string& key, const Pose& pose) {
  return nlohmann::json::array({key, numbers_of(pose.position), numbers_of(quat_to_euler_degrees(pose.rotation))});
}

}  // namespace

bool NetworkSession::send_pose(const std::string& key, Vec3 position, Quat rotation) {
  if (!can_send_pose(key)) return false;
  return deliver(pose_message(encoded(poses_.sent.update(key, position, rotation)), id_), kEveryone);
}

void NetworkSession::hear_from_game(const nlohmann::json& message, std::vector<NetworkEvent>& events) {
  const int from = message.value(kFrom, 0);
  if (message[kKind] == kPose) return hear_pose(from, message[kData].get_binary(), events);
  events.push_back(NetworkEvent::message(from, message[kName], message.value(kData, nlohmann::json())));
}

void NetworkSession::hear_pose(int from, const std::vector<std::uint8_t>& pose, std::vector<NetworkEvent>& events) {
  const auto update = decoded(pose);
  if (!update) return;  // not a pose: nothing of it is used
  if (const auto whole = poses_.heard.take(from, *update)) {
    events.push_back(NetworkEvent::message(from, kPoseMessageName, placement(update->key, *whole)));
  }
}

}  // namespace redgem::net
