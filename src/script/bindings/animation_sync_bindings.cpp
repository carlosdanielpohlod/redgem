// A character's own animation between machines (NetworkSync): what its owner's pose carries of
// it, and GameObject#__animation_sync, which makes a watched character do what it says.

#include "script/bindings/animation_sync.hpp"

#include <mruby/array.h>

#include <string>

#include "net/network_session.hpp"
#include "scene/animation/animation_player.hpp"
#include "script/bindings/network_bindings.hpp"
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
constexpr std::uint32_t kNeverAnimated = 0;  // the revision of a character no script animated
constexpr int kSyncArgs = 6;                 // fade, layer weight, own clip, mixed clip, graph, how late
constexpr const char* kNoClip = "a clip is [name, time, speed, loops, plays]";

AnimationPlayer& animations(mrb_state* mrb) {
  return required(mrb, context(mrb).animations, "animation sync is only available while a game runs");
}

std::optional<net::ClipPart> part_of(const std::optional<ClipState>& clip) {
  if (!clip) return std::nullopt;
  return net::ClipPart{clip->clip, clip->time, clip->speed, clip->loop, clip->playing};
}

// Whether the others are to hear what a character's animation does with the next pose of `key`.
// Nothing while its scripts never set any (the others' scripts then keep the clips they gave it),
// nor when they already heard what it does: then it is not even worked out.
bool worth_telling(mrb_state* mrb, ObjectId id, const std::string& key) {
  const std::uint32_t revision = animations(mrb).revision_of(scene(mrb), id);
  return revision != kNeverAnimated && network(mrb).tells_animation(key, revision);
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
  mrb_value own, layer, graph;
  mrb_get_args(mrb, "ffooof", &fade, &weight, &own, &layer, &graph, &late);
  CharacterAnimation state;
  state.fade = static_cast<float>(at_least(mrb, fade, 0.0, "the fade"));
  state.layer_weight = static_cast<float>(from_to(mrb, weight, 0.0, 1.0, "the layer weight"));
  state.own = clip_arg(mrb, own);
  state.layer = clip_arg(mrb, layer);
  state.graph = animation_graph_arg(mrb, graph);
  if (state.graph && state.own)
    mrb_raise(mrb, E_ARGUMENT_ERROR, "graph and manual clips cannot be synchronized together");
  const float late_by = static_cast<float>(at_least(mrb, late, 0.0, "how late it is"));
  animations(mrb).take_animation(scene(mrb), alive_id(mrb, self), state, late_by);
  return mrb_nil_value();
}

}  // namespace

std::optional<ClipState> animation_clip_arg(mrb_state* mrb, mrb_value value) { return clip_arg(mrb, value); }

std::optional<net::Animated> animated_of(mrb_state* mrb, mrb_value object, const std::string& key) {
  if (mrb_nil_p(object)) return std::nullopt;
  const ObjectId id = alive_id(mrb, object);
  if (!worth_telling(mrb, id, key)) return std::nullopt;
  const CharacterAnimation state = animations(mrb).animation_of(scene(mrb), id);
  net::AnimationPart part{state.fade, state.layer_weight, part_of(state.own), part_of(state.layer), {}};
  if (state.graph) part.graph = animation_graph_part(*state.graph);
  if (!net::carriable(part)) return std::nullopt;
  return net::Animated{state.revision, part};
}

void install_animation_sync_bindings(mrb_state* mrb) {
  RClass* game_object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, game_object, "__animation_sync", game_object_animation_sync, MRB_ARGS_REQ(kSyncArgs));
}

}  // namespace redgem
