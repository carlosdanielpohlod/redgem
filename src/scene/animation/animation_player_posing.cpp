// What every frame does: the clips move on and the objects are posed.

#include <iterator>

#include "scene/animation/animation.hpp"
#include "scene/animation/animation_player.hpp"

namespace redgem {
namespace {

constexpr std::uint64_t kAnimationPaths = 3;  // translation, rotation, scale

// What tells one animated part of one object from every other.
std::uint64_t part_key(ObjectId object, AnimationPath path) {
  return static_cast<std::uint64_t>(object) * kAnimationPaths + static_cast<std::uint64_t>(path);
}

// Copies the part of a pose that `path` is about.
void copy_part(const Transform& from, AnimationPath path, Transform& to) {
  if (path == AnimationPath::Translation) to.position = from.position;
  if (path == AnimationPath::Rotation) to.rotation = from.rotation;
  if (path == AnimationPath::Scale) to.scale = from.scale;
}

}  // namespace

void AnimationPlayer::advance(float dt, Scene& scene) {
  if (clips_ == nullptr) return;
  posed_.clear();
  for (std::size_t i = 0; i < clocks_.size(); ++i) redgem::advance(clocks_[i], duration_of(i), dt);
  pose_followers(scene);
  for (auto entry = owned_.begin(); entry != owned_.end();) {
    const ObjectId character = scene.resolve(entry->first);
    const bool done = character == kNoObject || advance_own(character, entry->second, dt, scene);
    entry = done ? owned_.erase(entry) : std::next(entry);
  }
}

void AnimationPlayer::pose_followers(Scene& scene) {
  // Characters with a clip of their own are left out of the scene's.
  PoseOptions everyone_else;
  if (!owned_.empty()) {
    everyone_else.wanted = [this, &scene](ObjectId object) { return owner_of(scene, object) == kNoObject; };
  }
  apply_scene_clips(scene, everyone_else);
}

void AnimationPlayer::apply_scene_clips(Scene& scene, PoseOptions options) const {
  for (std::size_t i = 0; i < clocks_.size(); ++i) {
    if (!clocks_[i].posing) continue;
    options.loop = clocks_[i].loop;
    apply_animation((*clips_)[i], clocks_[i].time, scene, options);
  }
}

void AnimationPlayer::advance_track(std::optional<Track>& track, float dt) const {
  if (track) redgem::advance(track->clock, duration_of(track->clip), dt);
}

bool AnimationPlayer::advance_own(ObjectId character, Own& own, float dt, Scene& scene) {
  for (std::optional<Track>* track : {&own.from, &own.to, &own.layer}) advance_track(*track, dt);
  own.faded += dt;
  const bool arrived = own.fade <= 0.0f || own.faded >= own.fade;
  if (!arrived) pose_character(character, own.from, kWhole, scene);
  pose_character(character, own.to, arrived ? kWhole : own.faded / own.fade, scene);
  if (own.layer) pose_character(character, own.layer, own.layer_weight, scene);
  if (!arrived) return false;
  own.from = own.to;
  // Back with the scene's clips and nothing laid over them: nothing of its own is left.
  const bool only_follows_the_scene = !own.to && !own.layer;
  return only_follows_the_scene;
}

void AnimationPlayer::pose_character(ObjectId character, const std::optional<Track>& track, float weight,
                                     Scene& scene) {
  PoseOptions options = character_options(character, weight, scene);
  if (!track) return apply_scene_clips(scene, options);
  options.loop = track->clock.loop;
  options.offsets = false;  // its own clip, at its own time
  apply_animation((*clips_)[track->clip], track->clock.time, scene, options);
}

PoseOptions AnimationPlayer::character_options(ObjectId character, float weight, const Scene& scene) {
  PoseOptions options;
  options.weight = weight;
  options.wanted = [this, &scene, character](ObjectId object) { return owner_of(scene, object) == character; };
  options.before = [this, &scene, weight](ObjectId object, AnimationPath path, Transform& local) {
    before_posing(scene, object, path, local, weight);
  };
  return options;
}

void AnimationPlayer::before_posing(const Scene& scene, ObjectId object, AnimationPath path, Transform& local,
                                    float weight) {
  if (weight < kWhole) {
    steady_base(scene, object, path, local);
  } else {
    posed_.insert(part_key(object, path));
  }
}

void AnimationPlayer::steady_base(const Scene& scene, ObjectId object, AnimationPath path, Transform& local) {
  if (!posed_.insert(part_key(object, path)).second) return;  // already posed this frame: blend from that
  // A copy rests as the object it copies does.
  const ObjectId original = scene.at(object).copy_of != kNoObject ? scene.at(object).copy_of : object;
  if (!is_index_of(original, rest_.size())) return;
  copy_part(rest_[static_cast<std::size_t>(original)], path, local);
}

}  // namespace redgem
