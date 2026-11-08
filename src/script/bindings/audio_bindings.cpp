// Audio natives: sounds (each with a handle), music (crossfaded) and the mix groups they play in.

#include <mruby.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_float kSilent = 0.0;

AudioSystem& audio(mrb_state* mrb) {
  return required(mrb, context(mrb).audio, "audio is only available while a game runs");
}

// A pitch a sound may play at, from kMinSpatialPitch to kMaxSpatialPitch.
float sound_pitch(mrb_state* mrb, mrb_float pitch, const char* what) {
  return static_cast<float>(from_to(mrb, pitch, kMinSpatialPitch, kMaxSpatialPitch, what));
}

// The group a sound is mixed in, named :music, :effects or :voice.
MixGroup mix_group(mrb_state* mrb, mrb_value name) {
  const std::string group = to_name(mrb, name);
  return known(mrb, mix_group_from_name(group), "unknown mix group :%s (music, effects, voice)", group);
}

// A sound's handle, or nil when it does not play.
mrb_value source_value(mrb_state* mrb, SoundSourceId id) {
  return id == kNoSoundSource ? mrb_nil_value() : mrb_int_value(mrb, static_cast<mrb_int>(id));
}

// Audio.__play(name, volume, pitch, loop, group): the handle of a flat sound, or nil.
mrb_value audio_play(mrb_state* mrb, mrb_value) {
  mrb_value name, group;
  mrb_float volume = 0.0, pitch = 0.0;
  mrb_bool loop = false;
  mrb_get_args(mrb, "offbo", &name, &volume, &pitch, &loop, &group);
  FlatSound sound;
  sound.volume = static_cast<float>(at_least(mrb, volume, kSilent, "Audio.play's volume"));
  sound.pitch = sound_pitch(mrb, pitch, "Audio.play's pitch");
  sound.loop = loop;
  sound.group = mix_group(mrb, group);
  return source_value(mrb, audio(mrb).play_sound(to_name(mrb, name), sound));
}

// Audio.__music(name, loop, volume, fade).
mrb_value audio_music(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool loop = true;
  mrb_float volume = 0.0, fade = 0.0;
  mrb_get_args(mrb, "obff", &name, &loop, &volume, &fade);
  MusicRequest request;
  request.loop = loop;
  request.volume = static_cast<float>(at_least(mrb, volume, kSilent, "Audio.music's volume"));
  request.fade_seconds = static_cast<float>(at_least(mrb, fade, kNoFade, "Audio.music's fade"));
  return mrb_bool_value(audio(mrb).play_music(to_name(mrb, name), request));
}

mrb_value audio_set_music_pitch(mrb_state* mrb, mrb_value) {
  const mrb_float pitch = more_than(mrb, float_arg(mrb), kSilent, "Audio.music_pitch");
  audio(mrb).set_music_pitch(static_cast<float>(pitch));
  return mrb_float_value(mrb, pitch);
}

mrb_value audio_set_music_volume(mrb_state* mrb, mrb_value) {
  const mrb_float volume = at_least(mrb, float_arg(mrb), kSilent, "Audio.music_volume");
  audio(mrb).set_music_volume(static_cast<float>(volume));
  return mrb_float_value(mrb, volume);
}

// Audio.__stop_music(fade).
mrb_value audio_stop_music(mrb_state* mrb, mrb_value) {
  audio(mrb).fade_out_music(static_cast<float>(at_least(mrb, float_arg(mrb), kNoFade, "Audio.stop_music's fade")));
  return mrb_nil_value();
}

mrb_value audio_set_volume(mrb_state* mrb, mrb_value) {
  const mrb_float volume = at_least(mrb, float_arg(mrb), kSilent, "Audio.volume");
  audio(mrb).set_master_volume(static_cast<float>(volume));
  return mrb_float_value(mrb, volume);
}

enum SpatialOption { Volume, Pitch, ReferenceDistance, MaxDistance, Loop, Group, SpatialOptionCount };
constexpr mrb_int kMaximumSourceId = std::numeric_limits<std::int32_t>::max();
struct SpatialArguments {
  mrb_value name{};
  mrb_value position{};
  std::array<mrb_value, SpatialOptionCount> options{};
};
std::array<mrb_sym, SpatialOptionCount> spatial_option_names(mrb_state* mrb) {
  return {mrb_intern_lit(mrb, "volume"),       mrb_intern_lit(mrb, "pitch"), mrb_intern_lit(mrb, "reference_distance"),
          mrb_intern_lit(mrb, "max_distance"), mrb_intern_lit(mrb, "loop"),  mrb_intern_lit(mrb, "group")};
}
SpatialArguments spatial_arguments(mrb_state* mrb) {
  SpatialArguments arguments;
  const auto names = spatial_option_names(mrb);
  const mrb_kwargs keywords{SpatialOptionCount, 0, names.data(), arguments.options.data(), nullptr};
  mrb_get_args(mrb, "oo:", &arguments.name, &arguments.position, &keywords);
  return arguments;
}
float spatial_number(mrb_state* mrb, mrb_value value, float fallback) {
  if (mrb_undef_p(value)) return fallback;
  return static_cast<float>(finite(mrb, mrb_as_float(mrb, value)));
}
bool spatial_loop(mrb_state* mrb, mrb_value value) {
  if (mrb_undef_p(value)) return false;
  if (mrb_true_p(value)) return true;
  if (mrb_false_p(value)) return false;
  mrb_raise(mrb, E_ARGUMENT_ERROR, "Audio.play_at's loop must be true or false");
  return false;
}
MixGroup spatial_group(mrb_state* mrb, mrb_value value) {
  return mrb_undef_p(value) ? MixGroup::Effects : mix_group(mrb, value);
}
SpatialSound spatial_sound(mrb_state* mrb, const SpatialArguments& arguments) {
  SpatialSound sound;
  sound.position = to_vec3(mrb, arguments.position);
  sound.volume = spatial_number(mrb, arguments.options[Volume], sound.volume);
  sound.pitch = sound_pitch(mrb, spatial_number(mrb, arguments.options[Pitch], sound.pitch), "Audio.play_at's pitch");
  sound.group = spatial_group(mrb, arguments.options[Group]);
  sound.reference_distance = spatial_number(mrb, arguments.options[ReferenceDistance], sound.reference_distance);
  sound.max_distance = spatial_number(mrb, arguments.options[MaxDistance], sound.max_distance);
  sound.loop = spatial_loop(mrb, arguments.options[Loop]);
  if (!valid_spatial_sound(sound))
    mrb_raise(mrb, E_ARGUMENT_ERROR,
              "Audio.play_at requires nonnegative volume, positive pitch and 0 < reference_distance < max_distance");
  return sound;
}
mrb_value audio_play_at(mrb_state* mrb, mrb_value) {
  const auto arguments = spatial_arguments(mrb);
  const auto sound = spatial_sound(mrb, arguments);
  return source_value(mrb, audio(mrb).play_spatial_sound(to_name(mrb, arguments.name), sound));
}
mrb_value audio_listener(mrb_state* mrb, mrb_value) {
  mrb_value position, forward = mrb_undef_value(), up = mrb_undef_value();
  mrb_get_args(mrb, "o|oo", &position, &forward, &up);
  AudioListener listener;
  listener.position = to_vec3(mrb, position);
  if (!mrb_undef_p(forward)) listener.forward = to_vec3(mrb, forward);
  if (!mrb_undef_p(up)) listener.up = to_vec3(mrb, up);
  if (!valid_audio_listener(listener))
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Audio.listener requires nonzero, nonparallel forward and up directions");
  return mrb_bool_value(audio(mrb).set_listener(listener));
}
SoundSourceId spatial_source_id(mrb_state* mrb, mrb_value value) {
  if (mrb_type(value) != MRB_TT_INTEGER) mrb_raise(mrb, E_ARGUMENT_ERROR, "expected a sound source integer id");
  const auto number = mrb_integer(value);
  if (number < 0) mrb_raise(mrb, E_ARGUMENT_ERROR, "sound source id must be nonnegative");
  if (number > kMaximumSourceId) mrb_raise(mrb, E_ARGUMENT_ERROR, "sound source id is out of range");
  return static_cast<SoundSourceId>(number);
}
mrb_value audio_move(mrb_state* mrb, mrb_value) {
  mrb_value id, position;
  mrb_get_args(mrb, "oo", &id, &position);
  return mrb_bool_value(audio(mrb).move_sound(spatial_source_id(mrb, id), to_vec3(mrb, position)));
}
mrb_value audio_playing(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(audio(mrb).sound_playing(spatial_source_id(mrb, single_arg(mrb))));
}
mrb_value audio_stop(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(audio(mrb).stop_sound(spatial_source_id(mrb, single_arg(mrb))));
}
// Audio.__set(id, volume, pitch): nil leaves that one as it is.
mrb_value audio_set(mrb_state* mrb, mrb_value) {
  mrb_value id, volume, pitch;
  mrb_get_args(mrb, "ooo", &id, &volume, &pitch);
  SoundLevels levels;
  if (!mrb_nil_p(volume))
    levels.volume = static_cast<float>(at_least(mrb, mrb_as_float(mrb, volume), kSilent, "Audio.set's volume"));
  if (!mrb_nil_p(pitch)) levels.pitch = sound_pitch(mrb, mrb_as_float(mrb, pitch), "Audio.set's pitch");
  return mrb_bool_value(audio(mrb).set_sound_levels(spatial_source_id(mrb, id), levels));
}
mrb_value audio_stop_all(mrb_state* mrb, mrb_value) {
  audio(mrb).stop_sounds();
  return mrb_nil_value();
}
void install_spatial_audio_bindings(mrb_state* mrb, RClass* module) {
  mrb_define_module_function(mrb, module, "play_at", audio_play_at,
                             MRB_ARGS_REQ(2) | MRB_ARGS_KEY(SpatialOptionCount, 0));
  mrb_define_module_function(mrb, module, "listener", audio_listener, MRB_ARGS_ARG(1, 2));
  mrb_define_module_function(mrb, module, "move", audio_move, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, module, "playing?", audio_playing, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, module, "stop", audio_stop, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, module, "__set", audio_set, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, module, "stop_sources", audio_stop_all, MRB_ARGS_NONE());
}

}  // namespace

void install_audio_bindings(mrb_state* mrb) {
  RClass* audio_module = mrb_define_module(mrb, "Audio");
  install_spatial_audio_bindings(mrb, audio_module);
  mrb_define_module_function(mrb, audio_module, "__play", audio_play, MRB_ARGS_REQ(5));
  mrb_define_module_function(mrb, audio_module, "__music", audio_music, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, audio_module, "__stop_music", audio_stop_music, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "music_pitch=", audio_set_music_pitch, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "music_volume=", audio_set_music_volume, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "volume=", audio_set_volume, MRB_ARGS_REQ(1));
}

}  // namespace redgem
