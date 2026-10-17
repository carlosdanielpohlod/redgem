// A character's own clip: what scripts ask of one object's animation.

#include <algorithm>

#include "scene/animation/animation_player.hpp"

namespace redgem {
namespace {

// Whether starting a clip `how` changes what a character does. `current`: the clock of that clip
// when the character is on it already (null otherwise), which carries on as it was unless asked
// to start over or to play at another pace.
bool changes_what_it_plays(const ClipClock* current, const ClipStart& how) {
  if (current == nullptr) return true;
  return how.restart || !current->playing || current->loop != how.loop || current->speed != how.speed;
}

// Whether mixing a clip over a character changes how it is mixed. `current`: the clock of that
// clip when it is mixed already (null otherwise).
bool mixes_otherwise(const ClipClock* current, float weight, float old_weight, bool loop, float speed) {
  if (current == nullptr) return true;
  return weight != old_weight || current->loop != loop || current->speed != speed;
}

}  // namespace

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

ClipClock* AnimationPlayer::clock_on(std::optional<Track>& track, std::size_t clip) {
  if (!track) return nullptr;
  return track->clip == clip ? &track->clock : nullptr;
}

AnimationPlayer::Track* AnimationPlayer::own_track(const Scene& scene, ObjectId object) {
  Own* own = own_of(scene, object);
  if (own == nullptr) return nullptr;
  return own->to ? &*own->to : nullptr;
}

AnimationPlayer::Track* AnimationPlayer::mixed_track(const Scene& scene, ObjectId object) {
  Own* own = own_of(scene, object);
  if (own == nullptr) return nullptr;
  return own->layer ? &*own->layer : nullptr;
}

bool AnimationPlayer::play_on(const Scene& scene, ObjectId object, const std::string& name, const ClipStart& how) {
  const auto index = find(name);
  if (!index) return false;
  Own& own = owned_[scene.handle(character_of(scene, object))];
  ClipClock* current = clock_on(own.to, *index);
  if (changes_what_it_plays(current, how)) changed(scene, object);
  if (current != nullptr) {
    start(*current, duration_of(*index), how.loop, how.speed, how.restart);  // no fade to itself
    return true;
  }
  Track next{*index, ClipClock{}};
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
  Track* track = own_track(scene, object);
  if (track == nullptr) return;
  if (!track->clock.playing) return;
  changed(scene, object);
  track->clock.playing = false;
}

bool AnimationPlayer::mix_on(const Scene& scene, ObjectId object, const std::string& name, float weight, bool loop,
                             float speed) {
  const auto index = find(name);
  if (!index) return false;
  Own& own = owned_[scene.handle(character_of(scene, object))];
  const ClipClock* mixed = clock_on(own.layer, *index);  // the same clip again: only mixed otherwise
  const bool another = mixed == nullptr;
  const float kept = std::clamp(weight, 0.0f, kWhole);  // what the mix takes it as
  if (mixes_otherwise(mixed, kept, own.layer_weight, loop, speed)) changed(scene, object);
  Track layer = own.layer.value_or(Track{});
  if (another) layer = Track{};
  layer.clip = *index;
  start(layer.clock, duration_of(*index), loop, speed, another);
  own.layer = layer;
  own.layer_weight = kept;
  return true;
}

void AnimationPlayer::unmix_on(const Scene& scene, ObjectId object) {
  Own* own = own_of(scene, object);
  if (own == nullptr) return;
  if (!own->layer) return;
  changed(scene, object);
  own->layer.reset();
}

void AnimationPlayer::follow_scene(const Scene& scene, ObjectId object, float fade) {
  if (track_on(scene, object) == nullptr) return;
  changed(scene, object);
  fade_to(*own_of(scene, object), std::nullopt, fade);
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
