// A character's own clip: what scripts ask of one object's animation.

#include <algorithm>

#include "scene/animation_player.hpp"

namespace redgem {

ObjectId AnimationPlayer::character_of(const Scene& scene, ObjectId object) {
  const ObjectId root = scene.at(object).copy_root;
  return root != kNoObject ? root : object;
}

ObjectId AnimationPlayer::owner_of(const Scene& scene, ObjectId object) const {
  const ObjectId root = scene.at(object).copy_root;
  if (root != kNoObject) return owned_.count(scene.handle(root)) != 0 ? root : kNoObject;
  for (ObjectId walk = object; walk != kNoObject; walk = scene.at(walk).parent) {
    if (owned_.count(scene.handle(walk)) != 0) return walk;
  }
  return kNoObject;
}

AnimationPlayer::Own* AnimationPlayer::own_of(const Scene& scene, ObjectId object) {
  const auto found = owned_.find(scene.handle(character_of(scene, object)));
  return found == owned_.end() ? nullptr : &found->second;
}

const AnimationPlayer::Own* AnimationPlayer::own_of(const Scene& scene, ObjectId object) const {
  const auto found = owned_.find(scene.handle(character_of(scene, object)));
  return found == owned_.end() ? nullptr : &found->second;
}

const AnimationPlayer::Track* AnimationPlayer::track_on(const Scene& scene, ObjectId object) const {
  const Own* own = own_of(scene, object);
  if (own == nullptr) return nullptr;
  return own->to ? &*own->to : nullptr;
}

bool AnimationPlayer::play_on(const Scene& scene, ObjectId object, const std::string& name, const ClipStart& how) {
  const auto index = find(name);
  if (!index) return false;
  Own& own = owned_[scene.handle(character_of(scene, object))];
  const bool already_on_it = own.to && own.to->clip == *index;
  if (already_on_it) {
    // Carry on (or start over), with no fade to itself.
    start(own.to->clock, duration_of(*index), how.loop, how.speed, how.restart);
    return true;
  }
  Track next;
  next.clip = *index;
  start(next.clock, duration_of(*index), how.loop, how.speed, true);
  fade_to(own, next, how.fade);
  return true;
}

void AnimationPlayer::fade_to(Own& own, const std::optional<Track>& next, float fade) {
  // Mid-fade, the change starts from where the character was heading.
  own.from = own.to;
  own.to = next;
  own.fade = std::max(fade, 0.0f);
  own.faded = 0.0f;
}

void AnimationPlayer::pause_on(const Scene& scene, ObjectId object) {
  Own* own = own_of(scene, object);
  if (own == nullptr) return;
  if (own->to) own->to->clock.playing = false;
}

bool AnimationPlayer::mix_on(const Scene& scene, ObjectId object, const std::string& name, float weight, bool loop,
                             float speed) {
  const auto index = find(name);
  if (!index) return false;
  Own& own = owned_[scene.handle(character_of(scene, object))];
  // The same clip again only changes how it is mixed; another one starts from its beginning.
  const bool another = !own.layer || own.layer->clip != *index;
  Track layer = own.layer.value_or(Track{});
  if (another) layer = Track{};
  layer.clip = *index;
  start(layer.clock, duration_of(*index), loop, speed, another);
  own.layer = layer;
  own.layer_weight = std::clamp(weight, 0.0f, kWhole);
  return true;
}

void AnimationPlayer::unmix_on(const Scene& scene, ObjectId object) {
  if (Own* own = own_of(scene, object)) own->layer.reset();
}

void AnimationPlayer::follow_scene(const Scene& scene, ObjectId object, float fade) {
  Own* own = own_of(scene, object);
  if (own == nullptr) return;
  if (own->to) fade_to(*own, std::nullopt, fade);
}

std::optional<std::string> AnimationPlayer::clip_on(const Scene& scene, ObjectId object) const {
  const Track* track = track_on(scene, object);
  return track != nullptr ? std::optional<std::string>((*clips_)[track->clip].name) : std::nullopt;
}

bool AnimationPlayer::playing_on(const Scene& scene, ObjectId object) const {
  const Track* track = track_on(scene, object);
  return track != nullptr && track->clock.playing;
}

float AnimationPlayer::time_on(const Scene& scene, ObjectId object) const {
  const Track* track = track_on(scene, object);
  return track != nullptr ? track->clock.time : 0.0f;
}

}  // namespace redgem
