// NetworkSession: what machines in a game hear from each other, and the poses of the objects they
// move (NetworkSync), sent in a format of their own (pose_codec.hpp), saying only what changed.

#include "net/network_session.hpp"

namespace redgem::net {

using namespace wire;

namespace {

nlohmann::json numbers_of(Vec3 vector) { return nlohmann::json::array({vector.x, vector.y, vector.z}); }

nlohmann::json clip_of(const std::optional<ClipPart>& clip) {
  if (!clip) return nullptr;
  return nlohmann::json::array({clip->clip, clip->time, clip->speed, clip->loop, clip->playing});
}

// A pose as the game's scripts hear it (kPoseMessageName).
nlohmann::json placement(const PoseUpdate& update, const Pose& pose) {
  nlohmann::json heard =
      nlohmann::json::array({update.key, numbers_of(pose.position), numbers_of(quat_to_euler_degrees(pose.rotation))});
  if (!update.animation) return heard;
  const AnimationPart& part = *update.animation;
  heard.push_back(nlohmann::json::array({part.fade, part.layer_weight, clip_of(part.own), clip_of(part.layer)}));
  return heard;
}

}  // namespace

bool NetworkSession::send_pose(const std::string& key, Vec3 position, Quat rotation,
                               const std::optional<Animated>& animated) {
  if (!can_send_pose(key, position, rotation)) return false;
  return deliver(pose_message(encoded(poses_.sent.update(key, position, rotation, animated)), id_), kEveryone);
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
    events.push_back(NetworkEvent::message(from, kPoseMessageName, placement(*update, *whole)));
  }
}

}  // namespace redgem::net
