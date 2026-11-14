// Root motion: a character's own clips moving the character itself. The root of each clip (the
// highest node it moves) keeps its rest place and heading, horizontally; how far it would have
// gone, and turned, goes to the character (take_root_motion), weighted as the clips are shown.

#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

#include "scene/animation/animation.hpp"
#include "scene/animation/animation_player.hpp"

namespace redgem {
namespace {

constexpr float kFullTurn = kTwo * kPi;

bool animates(const AnimationChannel& channel, AnimationPath path, const Scene& scene) {
  return channel.path == path && scene.is_alive(channel.node);
}

int depth_of(const Scene& scene, ObjectId object) {
  int depth = 0;
  for (ObjectId walk = scene.at(object).parent; walk != kNoObject; walk = scene.at(walk).parent) ++depth;
  return depth;
}

// The highest node of the scene a clip animates on `path`.
int highest_node(const AnimationDesc& clip, AnimationPath path, const Scene& scene) {
  int node = kNoIndex;
  int highest = INT_MAX;
  for (const AnimationChannel& channel : clip.channels) {
    if (!animates(channel, path, scene)) continue;
    const int depth = depth_of(scene, channel.node);
    if (depth < highest) node = channel.node;
    highest = std::min(highest, depth);
  }
  return node;
}

// A clip's root: the highest node it moves, or (for a clip that only turns things) turns. A node
// above that is only turned, such as an armature keyed for Blender's axes, is not it.
int root_node(const AnimationDesc& clip, const Scene& scene) {
  const int moving = highest_node(clip, AnimationPath::Translation, scene);
  return moving != kNoIndex ? moving : highest_node(clip, AnimationPath::Rotation, scene);
}

bool descends_from(const Scene& scene, ObjectId object, ObjectId ancestor) {
  for (ObjectId walk = scene.at(object).parent; walk != kNoObject; walk = scene.at(walk).parent) {
    if (walk == ancestor) return true;
  }
  return false;
}

// The copy of `original` inside the copy `character` is the root of.
ObjectId copy_inside(const Scene& scene, ObjectId original, ObjectId character) {
  for (const ObjectId copy : scene.copies_of(original)) {
    if (scene.at(copy).copy_root == character) return copy;
  }
  return kNoObject;
}

ObjectId shown_as(const Scene& scene, ObjectId original, ObjectId character) {
  if (scene.at(character).copy_root != kNoObject) return copy_inside(scene, original, character);
  return scene.is_alive(original) ? original : kNoObject;
}

Transform parent_world(const Scene& scene, ObjectId object) {
  const ObjectId parent = scene.at(object).parent;
  return parent == kNoObject ? Transform{} : scene.world_transform(parent);
}

Vec3 world_vector(const Transform& parent, Vec3 local) {
  return rotate(parent.rotation, hadamard(parent.scale, local));
}
Vec3 local_vector(const Transform& parent, Vec3 world) {
  return safe_divide(rotate(conjugate(parent.rotation), world), parent.scale);
}
Vec3 horizontal(Vec3 world) { return {world.x, 0.0f, world.z}; }
// The world's up, as the children of `parent` see it.
Vec3 up_in(const Transform& parent) { return rotate(conjugate(parent.rotation), Vec3::up()); }

float wrapped_angle(float radians) { return std::remainder(radians, kFullTurn); }

// How far `rotation` turns around `axis` (of length 1), within half a turn either way.
float twist_around(Quat rotation, Vec3 axis) {
  const float along = dot(Vec3{rotation.x, rotation.y, rotation.z}, axis);
  return wrapped_angle(kTwo * std::atan2(along, rotation.w));
}

Vec3 position_at(const AnimationChannel& channel, double time) {
  return vec3_from(sample_channel(channel, static_cast<float>(time)).data());
}

// How a root's turning is told: around the world's up as its parent sees it, from its rest.
struct Heading {
  Vec3 up;
  Quat rest;
};

float heading_at(const AnimationChannel& channel, double time, const Heading& heading) {
  const Quat rotation = quat_from(sample_channel(channel, static_cast<float>(time)).data());
  return twist_around(rotation * conjugate(heading.rest), heading.up);
}

// How far a translation channel went over a span, a whole lap of the clip for each lap round.
Vec3 moved(const AnimationChannel& channel, const ClockSpan& span, float duration) {
  const Vec3 lap = position_at(channel, duration) - position_at(channel, 0.0);
  return position_at(channel, span.to) - position_at(channel, span.from) + lap * static_cast<float>(span.laps);
}

// How far a rotation channel turned over a span.
float turned(const AnimationChannel& channel, const ClockSpan& span, float duration, const Heading& heading) {
  const float lap = wrapped_angle(heading_at(channel, duration, heading) - heading_at(channel, 0.0, heading));
  const float within = wrapped_angle(heading_at(channel, span.to, heading) - heading_at(channel, span.from, heading));
  return within + lap * static_cast<float>(span.laps);
}

// The channels that move and turn `node` in a clip (null for one it does not have).
std::pair<const AnimationChannel*, const AnimationChannel*> channels_of(const AnimationDesc& clip, int node) {
  std::pair<const AnimationChannel*, const AnimationChannel*> found{nullptr, nullptr};
  for (const AnimationChannel& channel : clip.channels) {
    if (channel.node != node) continue;
    if (channel.path == AnimationPath::Translation) found.first = &channel;
    if (channel.path == AnimationPath::Rotation) found.second = &channel;
  }
  return found;
}

}  // namespace

void AnimationPlayer::find_roots(const Scene& scene) {
  roots_.clear();
  for (const AnimationDesc& clip : *clips_) {
    const int node = root_node(clip, scene);
    const auto [moving, turning] = channels_of(clip, node);
    roots_.push_back(ClipRoot{node, moving, turning});
  }
}

void AnimationPlayer::set_root_motion(const Scene& scene, ObjectId object, bool on) {
  root_motion_[scene.handle(character_of(scene, object))].every_clip = on;
}

bool AnimationPlayer::root_motion(const Scene& scene, ObjectId object) const {
  const auto found = root_motion_.find(scene.handle(character_of(scene, object)));
  return found != root_motion_.end() && found->second.every_clip;
}

bool AnimationPlayer::set_clip_root_motion(const Scene& scene, ObjectId object, const std::string& name, bool on) {
  const auto index = find(name);
  if (!index) return false;
  std::set<std::size_t>& clips = root_motion_[scene.handle(character_of(scene, object))].clips;
  if (on) {
    clips.insert(*index);
  } else {
    clips.erase(*index);
  }
  return true;
}

std::vector<RootMotion> AnimationPlayer::take_root_motion() { return std::exchange(motions_, {}); }

bool AnimationPlayer::moves_root(const RootMotionChoice& choice, std::size_t clip) {
  return choice.every_clip || choice.clips.count(clip) != 0;
}

ObjectId AnimationPlayer::root_object(const Scene& scene, ObjectId character, int node) {
  if (node == kNoIndex) return kNoObject;
  const ObjectId shown = shown_as(scene, static_cast<ObjectId>(node), character);
  // (A clip that moves the character's own object is left as it is: that is the character.)
  const bool below_the_character = shown != kNoObject && descends_from(scene, shown, character);
  return below_the_character ? shown : kNoObject;
}

Transform AnimationPlayer::rest_of(const Scene& scene, ObjectId object) const {
  // A copy rests as the object it copies does.
  const ObjectId original = scene.at(object).copy_of != kNoObject ? scene.at(object).copy_of : object;
  if (!is_index_of(original, rest_.size())) return scene.at(object).local;
  return rest_[static_cast<std::size_t>(original)];
}

void AnimationPlayer::move_by_root(ObjectId character, const CharacterSteps& steps, Scene& scene) {
  const auto choice = root_motion_.find(scene.handle(character));
  if (choice == root_motion_.end()) return;
  RootMotion motion{scene.handle(character), Vec3{}, 0.0f};
  const bool from_moved = add_side(choice->second, steps.from, kWhole - steps.blend, scene, motion);
  const bool to_moved = add_side(choice->second, steps.to, steps.blend, scene, motion);
  // (Nothing with root motion shown: the character is left alone.)
  if (from_moved || to_moved) motions_.push_back(motion);
}

bool AnimationPlayer::add_side(const RootMotionChoice& choice, const std::vector<ClipStep>& side, float weight,
                               Scene& scene, RootMotion& motion) {
  bool moved = false;
  for (const ClipStep& step : side) {
    if (moves_root(choice, step.clip)) moved = add_step(step, weight * step.weight, scene, motion) || moved;
  }
  return moved;
}

bool AnimationPlayer::add_step(const ClipStep& step, float weight, Scene& scene, RootMotion& motion) {
  const ClipRoot& root = roots_[step.clip];
  const ObjectId object = root_object(scene, scene.resolve(motion.character), root.node);
  if (object == kNoObject) return false;
  const Transform parent = parent_world(scene, object);
  const float duration = duration_of(step.clip);
  if (root.translation != nullptr) {
    const Vec3 went = world_vector(parent, moved(*root.translation, step.span, duration));
    motion.translation = motion.translation + horizontal(went) * weight;
  }
  const Heading heading{up_in(parent), rest_of(scene, object).rotation};
  if (root.rotation != nullptr) motion.yaw += turned(*root.rotation, step.span, duration, heading) * weight;
  pin(scene, object, parent);
  return true;
}

void AnimationPlayer::pin(Scene& scene, ObjectId root, const Transform& parent) const {
  const Transform rest = rest_of(scene, root);
  Transform& local = scene.at(root).local;
  const Vec3 drift = horizontal(world_vector(parent, local.position - rest.position));
  local.position = local.position - local_vector(parent, drift);
  const Vec3 up = up_in(parent);
  const float off_heading = twist_around(local.rotation * conjugate(rest.rotation), up);
  local.rotation = normalize(quat_from_axis_angle(up, -off_heading) * local.rotation);
}

}  // namespace redgem
