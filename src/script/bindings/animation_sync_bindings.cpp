// A character's own animation between machines (NetworkSync): what its owner's pose carries of
// it, and GameObject#__animation_sync, which makes a watched character do what it says.

#include "script/bindings/animation_sync.hpp"

#include <mruby/array.h>

#include <string>

#include "scene/animation/animation_player.hpp"
#include "script/bindings/binding_helpers.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kClipParts = 5;  // name, time, speed, loops, plays
constexpr mrb_int kNameAt = 0;
constexpr mrb_int kTimeAt = 1;
constexpr mrb_int kSpeedAt = 2;
constexpr mrb_int kLoopsAt = 3;
constexpr mrb_int kPlaysAt = 4;
constexpr int kSyncArgs = 5;  // fade, layer weight, own clip, mixed clip, how late
constexpr const char* kNoClip = "a clip is [name, time, speed, loops, plays]";

AnimationPlayer& animations(mrb_state* mrb) {
  return required(mrb, context(mrb).animations, "animation sync is only available while a game runs");
}

std::optional<net::ClipPart> part_of(const std::optional<ClipState>& clip) {
  if (!clip) return std::nullopt;
  return net::ClipPart{clip->clip, clip->time, clip->speed, clip->loop, clip->playing};
}

bool is_clip(mrb_value clip) { return mrb_array_p(clip) && RARRAY_LEN(clip) == kClipParts; }

float number_at(mrb_state* mrb, mrb_value list, mrb_int at) {
  return static_cast<float>(finite(mrb, mrb_as_float(mrb, mrb_ary_ref(mrb, list, at))));
}

// A clip as a pose carried it ([name, time, speed, loops, plays]), or nothing for nil.
std::optional<ClipState> clip_arg(mrb_state* mrb, mrb_value clip) {
  if (mrb_nil_p(clip)) return std::nullopt;
  if (!is_clip(clip)) mrb_raise(mrb, E_ARGUMENT_ERROR, kNoClip);
  return ClipState{to_name(mrb, mrb_ary_ref(mrb, clip, kNameAt)), number_at(mrb, clip, kTimeAt),
                   number_at(mrb, clip, kSpeedAt), mrb_test(mrb_ary_ref(mrb, clip, kLoopsAt)),
                   mrb_test(mrb_ary_ref(mrb, clip, kPlaysAt))};
}

// game_object.__animation_sync(fade, layer_weight, own, layer, late_by): what a pose of the
// object carried, `late_by` seconds ago.
mrb_value game_object_animation_sync(mrb_state* mrb, mrb_value self) {
  mrb_float fade = 0.0, weight = 0.0, late = 0.0;
  mrb_value own, layer;
  mrb_get_args(mrb, "ffoof", &fade, &weight, &own, &layer, &late);
  CharacterAnimation state;
  state.fade = static_cast<float>(at_least(mrb, fade, 0.0, "the fade"));
  state.layer_weight = static_cast<float>(from_to(mrb, weight, 0.0, 1.0, "the layer weight"));
  state.own = clip_arg(mrb, own);
  state.layer = clip_arg(mrb, layer);
  const float late_by = static_cast<float>(at_least(mrb, late, 0.0, "how late it is"));
  animations(mrb).take_animation(scene(mrb), alive_id(mrb, self), state, late_by);
  return mrb_nil_value();
}

}  // namespace

std::optional<net::Animated> animated_of(mrb_state* mrb, mrb_value object) {
  if (mrb_nil_p(object)) return std::nullopt;
  const CharacterAnimation state = animations(mrb).animation_of(scene(mrb), alive_id(mrb, object));
  const net::AnimationPart part{state.fade, state.layer_weight, part_of(state.own), part_of(state.layer)};
  if (!net::carriable(part)) return std::nullopt;
  return net::Animated{state.revision, part};
}

void install_animation_sync_bindings(mrb_state* mrb) {
  RClass* game_object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, game_object, "__animation_sync", game_object_animation_sync, MRB_ARGS_REQ(kSyncArgs));
}

}  // namespace redgem
