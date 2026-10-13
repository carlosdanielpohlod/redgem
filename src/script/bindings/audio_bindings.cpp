// Audio natives: sounds and music.

#include <mruby.h>

#include <algorithm>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_float kSilent = 0.0;

AudioSystem& audio(mrb_state* mrb) { return *context(mrb).audio; }

mrb_value audio_play(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_float volume = 0.0, pitch = 0.0;
  mrb_get_args(mrb, "off", &name, &volume, &pitch);
  return mrb_bool_value(
      audio(mrb).play_sound(to_name(mrb, name), static_cast<float>(volume), static_cast<float>(pitch)));
}

mrb_value audio_music(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool loop = true;
  mrb_float volume = 0.0;
  mrb_get_args(mrb, "obf", &name, &loop, &volume);
  return mrb_bool_value(audio(mrb).play_music(to_name(mrb, name), loop, static_cast<float>(volume)));
}

mrb_value audio_set_music_pitch(mrb_state* mrb, mrb_value) {
  const mrb_float pitch = float_arg(mrb);
  if (pitch <= 0.0) mrb_raise(mrb, E_ARGUMENT_ERROR, "Audio.music_pitch must be positive");
  audio(mrb).set_music_pitch(static_cast<float>(pitch));
  return mrb_float_value(mrb, pitch);
}

mrb_value audio_set_music_volume(mrb_state* mrb, mrb_value) {
  const mrb_float volume = float_arg(mrb);
  audio(mrb).set_music_volume(static_cast<float>(volume < kSilent ? kSilent : volume));
  return mrb_float_value(mrb, volume);
}

mrb_value audio_stop_music(mrb_state* mrb, mrb_value) {
  audio(mrb).stop_music();
  return mrb_nil_value();
}

mrb_value audio_set_volume(mrb_state* mrb, mrb_value) {
  const mrb_float volume = float_arg(mrb);
  audio(mrb).set_master_volume(static_cast<float>(volume));
  return mrb_float_value(mrb, volume);
}

}  // namespace

void install_audio_bindings(mrb_state* mrb) {
  RClass* audio_module = mrb_define_module(mrb, "Audio");
  mrb_define_module_function(mrb, audio_module, "__play", audio_play, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, audio_module, "__music", audio_music, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, audio_module, "stop_music", audio_stop_music, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, audio_module, "music_pitch=", audio_set_music_pitch, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "music_volume=", audio_set_music_volume, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "volume=", audio_set_volume, MRB_ARGS_REQ(1));
}

}  // namespace redgem
