// A character's own clips as other machines see them: what its owner says it does, and the same
// made true here (NetworkSync).

#include <algorithm>

#include "scene/animation/animation_player.hpp"

namespace redgem {

void AnimationPlayer::changed(const Scene& scene, ObjectId object) {
  ++revisions_[scene.handle(character_of(scene, object))];
}

ClipState AnimationPlayer::state_of(const Track& track) const {
  const ClipClock& clock = track.clock;
  return {(*clips_)[track.clip].name, clock.time, clock.speed, clock.loop, clock.playing};
}

std::uint32_t AnimationPlayer::revision_of(const Scene& scene, ObjectId object) const {
  const auto revision = revisions_.find(scene.handle(character_of(scene, object)));
  return revision != revisions_.end() ? revision->second : 0;
}

CharacterAnimation AnimationPlayer::animation_of(const Scene& scene, ObjectId object) const {
  CharacterAnimation state;
  state.revision = revision_of(scene, object);
  const Own* own = own_of(scene, object);
  if (own == nullptr) return state;
  if (own->to) state.own = state_of(*own->to);
  state.fade = std::max(own->fade - own->faded, 0.0f);
  if (own->layer) state.layer = state_of(*own->layer);
  state.layer_weight = own->layer_weight;
  return state;
}

void AnimationPlayer::catch_up(Track& track, const ClipState& state, float late_by) const {
  track.clock = ClipClock{state.time, state.speed, state.playing, state.loop, true};
  redgem::advance(track.clock, duration_of(track.clip), late_by);
}

void AnimationPlayer::take_animation(const Scene& scene, ObjectId object, const CharacterAnimation& state,
                                     float late_by) {
  take_own(scene, object, state, late_by);
  take_layer(scene, object, state, late_by);
}

void AnimationPlayer::take_own(const Scene& scene, ObjectId object, const CharacterAnimation& state, float late_by) {
  if (!state.own) {
    follow_scene(scene, object, state.fade);
    return;
  }
  const ClipState& own = *state.own;
  if (!onto(scene, object, own, state.fade)) return;
  if (Track* track = own_track(scene, object)) catch_up(*track, own, late_by);
}

bool AnimationPlayer::onto(const Scene& scene, ObjectId object, const ClipState& own, float fade) {
  if (clip_on(scene, object) == own.clip) return true;                         // on it already: it only catches up
  return play_on(scene, object, own.clip, {own.loop, own.speed, fade, true});  // fading in as there
}

void AnimationPlayer::take_layer(const Scene& scene, ObjectId object, const CharacterAnimation& state, float late_by) {
  if (!state.layer) {
    unmix_on(scene, object);
    return;
  }
  const ClipState& layer = *state.layer;
  if (!mix_on(scene, object, layer.clip, state.layer_weight, layer.loop, layer.speed)) return;
  if (Track* track = mixed_track(scene, object)) catch_up(*track, layer, late_by);
}

}  // namespace redgem
