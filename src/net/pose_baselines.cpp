#include "net/pose_baselines.hpp"

namespace redgem::net {

PoseUpdate PoseSender::update(const std::string& key, Vec3 position, Quat rotation) {
  const std::uint32_t turned = packed_rotation(rotation);
  const auto sent = sent_.find(key);
  if (due_whole(sent)) return whole(key, position, turned);
  const auto steps = steps_between(sent->second.position, position);
  if (!steps) return whole(key, position, turned);  // too far for a move
  return change(key, sent->second, *steps, turned);
}

bool PoseSender::due_whole(std::unordered_map<std::string, Sent>::const_iterator sent) const {
  return sent == sent_.end() || sent->second.sends >= kSendsPerWholePose;
}

PoseUpdate PoseSender::whole(const std::string& key, Vec3 position, std::uint32_t rotation) {
  if (sent_.size() >= kMaxRememberedPoses) sent_.clear();
  sent_[key] = Sent{position, rotation};
  PoseUpdate update;
  update.key = key;
  update.position = position;
  update.rotation = rotation;
  return update;
}

PoseUpdate PoseSender::change(const std::string& key, Sent& sent, const PoseSteps& steps, std::uint32_t rotation) {
  PoseUpdate update;
  update.key = key;
  ++sent.sends;
  if (moves(steps)) {
    update.moved = steps;
    sent.position = moved_by(sent.position, steps);  // where the others now have it
  }
  if (rotation != sent.rotation) {
    update.rotation = rotation;
    sent.rotation = rotation;
  }
  return update;
}

std::optional<Pose> PoseReceiver::take(int from, const PoseUpdate& update) {
  const Source source{from, update.key};
  if (update.whole()) remember(source, update);
  const auto found = known_.find(source);
  if (found == known_.end()) return std::nullopt;
  apply(update, found->second);
  return Pose{found->second.position, unpacked_rotation(found->second.rotation)};
}

void PoseReceiver::apply(const PoseUpdate& update, Known& known) {
  if (update.position) known.position = *update.position;
  if (update.moved) known.position = moved_by(known.position, *update.moved);
  if (update.rotation) known.rotation = *update.rotation;
}

void PoseReceiver::remember(const Source& source, const PoseUpdate& update) {
  if (known_.size() >= kMaxRememberedPoses) known_.clear();
  known_[source] = Known{update.position.value(), update.rotation.value()};
}

void PoseReceiver::forget(int peer) {
  // (Sources are in the order of their machine's id, then key: a machine's are all together.)
  known_.erase(known_.lower_bound({peer, std::string()}), known_.lower_bound({peer + 1, std::string()}));
}

}  // namespace redgem::net
