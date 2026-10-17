#include "net/pose_baselines.hpp"

#include <iterator>

namespace redgem::net {

namespace {

// Whether an animation is news to the others: with a whole pose (which may be the first they
// hear of the object), or when it changed since they were last told.
bool tells(const PoseUpdate& update, const std::optional<std::uint32_t>& told, std::uint32_t revision) {
  return update.whole() || told != revision;
}

}  // namespace

PoseUpdate PoseSender::update(const std::string& key, Vec3 position, Quat rotation,
                              const std::optional<Animated>& animated) {
  PoseUpdate update = placed(key, position, rotation);
  if (animated) tell_animation(update, *animated);
  return update;
}

bool PoseSender::tells_animation(const std::string& key, std::uint32_t revision) const {
  const auto sent = sent_.find(key);
  return due_whole(sent) || sent->second.revision != revision;
}

void PoseSender::tell_animation(PoseUpdate& update, const Animated& animated) {
  Sent& sent = sent_[update.key];  // (placed has just made it, or found it)
  if (!tells(update, sent.revision, animated.revision)) return;
  update.animation = animated.part;
  sent.revision = animated.revision;
}

PoseUpdate PoseSender::placed(const std::string& key, Vec3 position, Quat rotation) {
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
  if (full_without(key)) sent_.clear();
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
  if (update.whole()) start(source);
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

void PoseReceiver::start(const Source& source) {
  if (full_without(source)) forget(source.first);
  known_[source] = Known();  // (what the whole pose says is applied next)
}

std::size_t PoseReceiver::poses_of(int peer) const {
  // (Sources are in the order of their machine's id, then key: a machine's are all together.)
  return static_cast<std::size_t>(
      std::distance(known_.lower_bound({peer, std::string()}), known_.lower_bound({peer + 1, std::string()})));
}

void PoseReceiver::forget(int peer) {
  // (Sources are in the order of their machine's id, then key: a machine's are all together.)
  known_.erase(known_.lower_bound({peer, std::string()}), known_.lower_bound({peer + 1, std::string()}));
}

}  // namespace redgem::net
