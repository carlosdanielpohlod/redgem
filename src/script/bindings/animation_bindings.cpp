// Animation natives: the scene's clips by name, and each object's own (GameObject#animation).

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "scene/animation_player.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

// --- Animation ---------------------------------------------------------------------------------

AnimationPlayer& animations(mrb_state* mrb) {
  AnimationPlayer* player = context(mrb).animations;
  if (player == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "Animation is only available while a game runs");
  return *player;
}

[[noreturn]] void raise_unknown_clip(mrb_state* mrb, const std::string& clip) {
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "the scene has no animation called '%s'", clip.c_str());
}

// The clip's name, which must be one of the scene's.
std::string clip_arg(mrb_state* mrb, mrb_value name) {
  const std::string clip = to_name(mrb, name);
  if (!animations(mrb).has(clip)) raise_unknown_clip(mrb, clip);
  return clip;
}

std::string clip_arg(mrb_state* mrb) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  return clip_arg(mrb, name);
}

mrb_value animation_names(mrb_state* mrb, mrb_value) {
  const mrb_value names = mrb_ary_new(mrb);
  for (const std::string& name : animations(mrb).names()) mrb_ary_push(mrb, names, mrb_str_new_cstr(mrb, name.c_str()));
  return names;
}

mrb_value animation_play(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool loop = true, restart = false;
  mrb_float speed = 1.0;
  mrb_get_args(mrb, "obfb", &name, &loop, &speed, &restart);
  animations(mrb).play(clip_arg(mrb, name), loop, static_cast<float>(speed), restart);
  return mrb_nil_value();
}

mrb_value animation_pause(mrb_state* mrb, mrb_value) {
  animations(mrb).pause(clip_arg(mrb));
  return mrb_nil_value();
}

mrb_value animation_stop(mrb_state* mrb, mrb_value) {
  animations(mrb).stop(clip_arg(mrb));
  return mrb_nil_value();
}

mrb_value animation_seek(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_float seconds = 0.0;
  mrb_get_args(mrb, "of", &name, &seconds);
  animations(mrb).seek(clip_arg(mrb, name), static_cast<float>(seconds));
  return mrb_nil_value();
}

mrb_value animation_playing(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(animations(mrb).playing(clip_arg(mrb)).value_or(false));
}
mrb_value animation_time(mrb_state* mrb, mrb_value) {
  return mrb_float_value(mrb, animations(mrb).time(clip_arg(mrb)).value_or(0.0f));
}
mrb_value animation_duration(mrb_state* mrb, mrb_value) {
  return mrb_float_value(mrb, animations(mrb).duration(clip_arg(mrb)).value_or(0.0f));
}

// --- The object's own animation (game_object.animation) ----------------------------------------

// The object a method was called on, which must still exist.
ObjectId character(mrb_state* mrb, mrb_value self) { return alive_object(mrb, self).id; }

mrb_value game_object_animation_play(mrb_state* mrb, mrb_value self) {
  mrb_value name;
  mrb_bool loop = true, restart = false;
  mrb_float speed = 1.0, fade = 0.0;
  mrb_get_args(mrb, "obffb", &name, &loop, &speed, &fade, &restart);
  const std::string clip = to_name(mrb, name);
  const ClipStart how{loop, static_cast<float>(speed), static_cast<float>(fade), restart};
  if (!animations(mrb).play_on(scene(mrb), character(mrb, self), clip, how)) raise_unknown_clip(mrb, clip);
  return mrb_nil_value();
}

mrb_value game_object_animation_mix(mrb_state* mrb, mrb_value self) {
  mrb_value name;
  mrb_bool loop = true;
  mrb_float weight = 0.0, speed = 1.0;
  mrb_get_args(mrb, "ofbf", &name, &weight, &loop, &speed);
  const std::string clip = to_name(mrb, name);
  const bool mixed = animations(mrb).mix_on(scene(mrb), character(mrb, self), clip, static_cast<float>(weight), loop,
                                            static_cast<float>(speed));
  if (!mixed) raise_unknown_clip(mrb, clip);
  return mrb_nil_value();
}

mrb_value game_object_animation_unmix(mrb_state* mrb, mrb_value self) {
  animations(mrb).unmix_on(scene(mrb), character(mrb, self));
  return mrb_nil_value();
}

mrb_value game_object_animation_pause(mrb_state* mrb, mrb_value self) {
  animations(mrb).pause_on(scene(mrb), character(mrb, self));
  return mrb_nil_value();
}

mrb_value game_object_animation_follow_scene(mrb_state* mrb, mrb_value self) {
  mrb_float fade = 0.0;
  mrb_get_args(mrb, "f", &fade);
  animations(mrb).follow_scene(scene(mrb), character(mrb, self), static_cast<float>(fade));
  return mrb_nil_value();
}

mrb_value game_object_animation_clip(mrb_state* mrb, mrb_value self) {
  const auto clip = animations(mrb).clip_on(scene(mrb), character(mrb, self));
  return clip ? mrb_str_new_cstr(mrb, clip->c_str()) : mrb_nil_value();
}

mrb_value game_object_animation_playing(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(animations(mrb).playing_on(scene(mrb), character(mrb, self)));
}

mrb_value game_object_animation_time(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, animations(mrb).time_on(scene(mrb), character(mrb, self)));
}

}  // namespace

namespace {

void install_scene_clip_bindings(mrb_state* mrb) {
  RClass* animation_module = mrb_define_module(mrb, "Animation");
  mrb_define_module_function(mrb, animation_module, "names", animation_names, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, animation_module, "__play", animation_play, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, animation_module, "pause", animation_pause, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, animation_module, "stop", animation_stop, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, animation_module, "seek", animation_seek, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, animation_module, "playing?", animation_playing, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, animation_module, "time", animation_time, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, animation_module, "duration", animation_duration, MRB_ARGS_REQ(1));
}

// One object's own animation (see Animator in the prelude).
void install_object_animation_bindings(mrb_state* mrb) {
  RClass* game_object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, game_object, "__animation_play", game_object_animation_play, MRB_ARGS_REQ(5));
  mrb_define_method(mrb, game_object, "__animation_mix", game_object_animation_mix, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, game_object, "__animation_unmix", game_object_animation_unmix, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__animation_pause", game_object_animation_pause, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__animation_follow_scene", game_object_animation_follow_scene, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "__animation_clip", game_object_animation_clip, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__animation_playing", game_object_animation_playing, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__animation_time", game_object_animation_time, MRB_ARGS_NONE());
}

}  // namespace

void install_animation_bindings(mrb_state* mrb) {
  install_scene_clip_bindings(mrb);
  install_object_animation_bindings(mrb);
}

}  // namespace redgem
