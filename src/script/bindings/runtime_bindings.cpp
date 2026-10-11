// Time, UI, Debug, Game and Audio natives.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "script/script_config.hpp"
#include "core/log.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kOpaque = 255;  // a color channel at its fullest

// --- Time ------------------------------------------------------------------------------------

mrb_value time_delta(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.delta); }
mrb_value time_fixed_delta(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.fixed_delta); }
mrb_value time_elapsed(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.elapsed); }
mrb_value time_frame(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).time.frame); }

mrb_value time_scale(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.scale); }
mrb_value time_set_scale(mrb_state* mrb, mrb_value) {
  mrb_float scale = 1.0;
  mrb_get_args(mrb, "f", &scale);
  if (scale < 0.0) mrb_raise(mrb, E_ARGUMENT_ERROR, "Time.scale cannot be negative");
  context(mrb).time.scale = static_cast<float>(scale);
  return mrb_float_value(mrb, scale);
}

// --- UI ----------------------------------------------------------------------------------------

mrb_value ui_rect(mrb_state* mrb, mrb_value) {
  mrb_int x = 0, y = 0, width = 0, height = 0, r = 0, g = 0, b = 0, a = 0;
  mrb_float radius = 0.0;
  mrb_get_args(mrb, "iiiiiiiif", &x, &y, &width, &height, &r, &g, &b, &a, &radius);
  HudItem item;
  item.kind = HudItem::Kind::Rect;
  item.x = static_cast<int>(x);
  item.y = static_cast<int>(y);
  item.width = static_cast<int>(width);
  item.height = static_cast<int>(height);
  item.radius = static_cast<float>(radius);
  item.color = color_from(r, g, b, a);
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

mrb_value ui_image(mrb_state* mrb, mrb_value) {
  const char* name = nullptr;
  mrb_int x = 0, y = 0, width = 0, height = 0, a = 0;
  mrb_get_args(mrb, "ziiiii", &name, &x, &y, &width, &height, &a);
  HudItem item;
  item.kind = HudItem::Kind::Image;
  item.text = name;
  item.x = static_cast<int>(x);
  item.y = static_cast<int>(y);
  item.width = static_cast<int>(width);
  item.height = static_cast<int>(height);
  item.color = color_from(kOpaque, kOpaque, kOpaque, a);
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

mrb_value ui_width(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).screen_width); }
mrb_value ui_height(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).screen_height); }

mrb_value ui_text_width(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_int size = 0;
  mrb_get_args(mrb, "zi", &text, &size);
  const ScriptContext& ctx = context(mrb);
  // Without a renderer (headless runs) there is no font to measure with: a usual width per letter.
  const int width = ctx.measure_text ? ctx.measure_text(text, static_cast<int>(size))
                                     : static_cast<int>(static_cast<float>(std::string(text).size()) *
                                                        static_cast<float>(size) * config::kEstimatedLetterWidth);
  return mrb_int_value(mrb, width);
}

mrb_value ui_text(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_int x = 0, y = 0, size = 0, r = 0, g = 0, b = 0, a = 0;
  mrb_get_args(mrb, "ziiiiiii", &text, &x, &y, &size, &r, &g, &b, &a);
  HudItem hud;
  hud.text = text;
  hud.x = static_cast<int>(x);
  hud.y = static_cast<int>(y);
  hud.size = static_cast<int>(size);
  hud.color = color_from(r, g, b, a);
  context(mrb).hud.push_back(std::move(hud));
  return mrb_nil_value();
}

// --- Debug -------------------------------------------------------------------------------------

std::string message_arg(mrb_state* mrb) {
  mrb_value message;
  mrb_get_args(mrb, "o", &message);
  return mrb_str_to_cstr(mrb, mrb_obj_as_string(mrb, message));
}

mrb_value debug_log(mrb_state* mrb, mrb_value) {
  log::info(message_arg(mrb));
  return mrb_nil_value();
}

mrb_value debug_warn(mrb_state* mrb, mrb_value) {
  log::warning(message_arg(mrb));
  return mrb_nil_value();
}

mrb_value debug_error(mrb_state* mrb, mrb_value) {
  log::error(message_arg(mrb));
  return mrb_nil_value();
}

// --- Game ----------------------------------------------------------------------------------------

mrb_value game_quit(mrb_state* mrb, mrb_value) {
  context(mrb).quit_requested = true;
  return mrb_nil_value();
}

mrb_value game_screenshot(mrb_state* mrb, mrb_value) {
  context(mrb).screenshot_requests.push_back(name_arg(mrb));
  return mrb_nil_value();
}

// --- Audio ----------------------------------------------------------------------------------------

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
  mrb_float pitch = 0.0;
  mrb_get_args(mrb, "f", &pitch);
  if (pitch <= 0.0) mrb_raise(mrb, E_ARGUMENT_ERROR, "Audio.music_pitch must be positive");
  audio(mrb).set_music_pitch(static_cast<float>(pitch));
  return mrb_float_value(mrb, pitch);
}

mrb_value audio_set_music_volume(mrb_state* mrb, mrb_value) {
  mrb_float volume = 0.0;
  mrb_get_args(mrb, "f", &volume);
  audio(mrb).set_music_volume(static_cast<float>(volume < 0.0 ? 0.0 : volume));
  return mrb_float_value(mrb, volume);
}

mrb_value audio_stop_music(mrb_state* mrb, mrb_value) {
  audio(mrb).stop_music();
  return mrb_nil_value();
}

mrb_value audio_set_volume(mrb_state* mrb, mrb_value) {
  mrb_float volume = 0.0;
  mrb_get_args(mrb, "f", &volume);
  audio(mrb).set_master_volume(static_cast<float>(volume));
  return mrb_float_value(mrb, volume);
}

// --- Scenes ---------------------------------------------------------------------------------------

mrb_value game_load_scene(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  ScriptContext& ctx = context(mrb);
  if (!ctx.scene_exists || !ctx.scene_exists(name))
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "no scene named %s", name.c_str());
  ctx.requested_scene = name;
  return mrb_nil_value();
}

mrb_value game_scene_name(mrb_state* mrb, mrb_value) { return mrb_str_new_cstr(mrb, context(mrb).scene_name.c_str()); }

}  // namespace

namespace {

// mruby-time already defines the Time class; the game clock lives on it as class methods.
void install_time_bindings(mrb_state* mrb) {
  RClass* time_class = mrb_define_class(mrb, "Time", mrb->object_class);
  mrb_define_class_method(mrb, time_class, "delta", time_delta, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "fixed_delta", time_fixed_delta, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "elapsed", time_elapsed, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "frame", time_frame, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "scale", time_scale, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "scale=", time_set_scale, MRB_ARGS_REQ(1));
}

void install_ui_bindings(mrb_state* mrb) {
  RClass* ui_module = mrb_define_module(mrb, "UI");
  mrb_define_module_function(mrb, ui_module, "__text", ui_text, MRB_ARGS_REQ(8));
  mrb_define_module_function(mrb, ui_module, "__rect", ui_rect, MRB_ARGS_REQ(9));
  mrb_define_module_function(mrb, ui_module, "__image", ui_image, MRB_ARGS_REQ(6));
  mrb_define_module_function(mrb, ui_module, "__text_width", ui_text_width, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, ui_module, "width", ui_width, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "height", ui_height, MRB_ARGS_NONE());
}

void install_debug_bindings(mrb_state* mrb) {
  RClass* debug_module = mrb_define_module(mrb, "Debug");
  mrb_define_module_function(mrb, debug_module, "log", debug_log, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, debug_module, "warn", debug_warn, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, debug_module, "error", debug_error, MRB_ARGS_REQ(1));
}

void install_game_bindings(mrb_state* mrb) {
  RClass* game_module = mrb_define_module(mrb, "Game");
  mrb_define_module_function(mrb, game_module, "quit", game_quit, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game_module, "screenshot", game_screenshot, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, game_module, "load_scene", game_load_scene, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, game_module, "scene_name", game_scene_name, MRB_ARGS_NONE());
}

void install_audio_bindings(mrb_state* mrb) {
  RClass* audio_module = mrb_define_module(mrb, "Audio");
  mrb_define_module_function(mrb, audio_module, "__play", audio_play, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, audio_module, "__music", audio_music, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, audio_module, "stop_music", audio_stop_music, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, audio_module, "music_pitch=", audio_set_music_pitch, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "music_volume=", audio_set_music_volume, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "volume=", audio_set_volume, MRB_ARGS_REQ(1));
}

}  // namespace

void install_runtime_bindings(mrb_state* mrb) {
  install_time_bindings(mrb);
  install_ui_bindings(mrb);
  install_debug_bindings(mrb);
  install_game_bindings(mrb);
  install_audio_bindings(mrb);
}

}  // namespace redgem
