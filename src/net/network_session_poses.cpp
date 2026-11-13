// NetworkSession: what machines in a game hear from each other, and the poses of the objects they
// move (NetworkSync), sent in a format of their own (pose_codec.hpp), saying only what changed.

#include "net/network_session.hpp"

#include <utility>

namespace redgem::net {

using namespace wire;

namespace {

nlohmann::json numbers_of(Vec3 vector) { return nlohmann::json::array({vector.x, vector.y, vector.z}); }

nlohmann::json clip_of(const std::optional<ClipPart>& clip) {
  if (!clip) return nullptr;
  return nlohmann::json::array({clip->clip, clip->time, clip->speed, clip->loop, clip->playing});
}

nlohmann::json graph_side(const std::vector<WeightedClipPart>& side) {
  auto result = nlohmann::json::array();
  for (const auto& entry : side) result.push_back(nlohmann::json::array({clip_of(entry.clip), entry.weight}));
  return result;
}

nlohmann::json animation_of(const AnimationPart& part) {
  auto result = nlohmann::json::array({part.fade, part.layer_weight, clip_of(part.own), clip_of(part.layer)});
  if (part.graph) {
    const auto& graph = *part.graph;
    result.push_back(nlohmann::json::array(
        {graph.blend, graph.remaining, graph.playing, graph_side(graph.from), graph_side(graph.to)}));
  }
  return result;
}

// A pose as the game's scripts hear it (kPoseMessageName).
nlohmann::json placement(const PoseUpdate& update, const Pose& pose) {
  nlohmann::json heard =
      nlohmann::json::array({update.key, numbers_of(pose.position), numbers_of(quat_to_euler_degrees(pose.rotation))});
  if (!update.animation) return heard;
  const AnimationPart& part = *update.animation;
  heard.push_back(animation_of(part));
  return heard;
}

// The pose an object is believed to be at, whole: what the others start from again once the host
// no longer passes on what its machine says (see NetworkSession::relay_pose).
PoseUpdate whole_pose(const PoseUpdate& update, const Pose& shown) {
  PoseUpdate whole;
  whole.key = update.key;
  whole.position = shown.position;
  whole.rotation = packed_rotation(shown.rotation);
  whole.animation = update.animation;
  return whole;
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

void NetworkSession::validate_movement(const MovementRules& rules) {
  movement_.configure(rules);
  diverged_.clear();
}

void NetworkSession::relay_checked_pose(Link& link, const nlohmann::json& passed, std::vector<NetworkEvent>& events) {
  const auto update = decoded(passed[kData].get_binary());
  if (!update) return;  // not a pose: nobody hears of it
  const auto claimed = poses_.heard.take(link.peer, *update);
  if (!claimed) {
    queue_for_everyone_but(link.peer, passed);  // nothing to check it against: as without validation
    return;
  }
  const MovementVerdict verdict = movement_.check(link.peer, update->key, claimed->position);
  const Pose shown{verdict.position, claimed->rotation};
  relay_pose(link.peer, *update, shown, passed, verdict.valid);
  events.push_back(NetworkEvent::message(link.peer, kPoseMessageName, placement(*update, shown)));
  if (!verdict.valid) correct(link, update->key, verdict.position, verdict.kick);
}

void NetworkSession::relay_pose(int from, const PoseUpdate& update, const Pose& shown, const nlohmann::json& passed,
                                bool valid) {
  const auto source = std::make_pair(from, update.key);
  if (passes_as_sent(source, update, valid)) {
    diverged_.erase(source);
    queue_for_everyone_but(from, passed);
    return;
  }
  if (diverged_.size() < kMaxDivergedObjects) diverged_.insert(source);
  queue_for_everyone_but(from, pose_message(encoded(whole_pose(update, shown)), from));
}

bool NetworkSession::passes_as_sent(const std::pair<int, std::string>& source, const PoseUpdate& update,
                                    bool valid) const {
  return valid && (update.whole() || diverged_.count(source) == 0);
}

void NetworkSession::correct(Link& link, const std::string& key, Vec3 position, bool kick) {
  link.connection.send(
      game_message(kCorrectionMessageName, nlohmann::json::array({key, numbers_of(position)}), kHostId, link.peer));
  if (kick) link.connection.close();  // past the game's kick_after: let go
}

void NetworkSession::forget_movement_of(int peer) {
  movement_.forget(peer);
  diverged_.erase(diverged_.lower_bound({peer, std::string()}), diverged_.lower_bound({peer + 1, std::string()}));
}

}  // namespace redgem::net
