// Input, Time, UI, Debug, Game and TestInput natives.

#include <mruby.h>
#include <mruby/string.h>

#include "core/log.hpp"
#include "input/keys.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr float kMaxColorChannel = 255.0f;

InputState& input(mrb_state* mrb) { return *context(mrb).input; }

Key key_arg(mrb_state* mrb) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  const std::string key_name = to_name(mrb, name);
  const auto key = key_from_name(key_name);
  if (!key) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown key :%s", key_name.c_str());
  return *key;
}

std::string name_arg(mrb_state* mrb) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  return to_name(mrb, name);
}

// --- Input ---------------------------------------------------------------------------------

mrb_value input_down(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).down(key_arg(mrb))); }
mrb_value input_pressed(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).pressed(key_arg(mrb))); }
mrb_value input_released(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).released(key_arg(mrb))); }

mrb_value input_axis(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  const auto value = input(mrb).axis(name);
  if (!value) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown axis :%s", name.c_str());
  return mrb_float_value(mrb, *value);
}

mrb_value input_action(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  const auto value = input(mrb).action_down(name);
  if (!value) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown action :%s", name.c_str());
  return mrb_bool_value(*value);
}

mrb_value input_action_pressed(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  const auto value = input(mrb).action_pressed(name);
  if (!value) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown action :%s", name.c_str());
  return mrb_bool_value(*value);
}

// --- Time ------------------------------------------------------------------------------------

mrb_value time_delta(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.delta); }
mrb_value time_fixed_delta(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.fixed_delta); }
mrb_value time_elapsed(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.elapsed); }
mrb_value time_frame(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).time.frame); }

// --- UI ----------------------------------------------------------------------------------------

mrb_value ui_text(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_int x = 0, y = 0, size = 0, r = 0, g = 0, b = 0, a = 0;
  mrb_get_args(mrb, "ziiiiiii", &text, &x, &y, &size, &r, &g, &b, &a);
  HudText hud;
  hud.text = text;
  hud.x = static_cast<int>(x);
  hud.y = static_cast<int>(y);
  hud.size = static_cast<int>(size);
  hud.color = {static_cast<float>(r) / kMaxColorChannel, static_cast<float>(g) / kMaxColorChannel,
               static_cast<float>(b) / kMaxColorChannel, static_cast<float>(a) / kMaxColorChannel};
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
  return mrb_bool_value(audio(mrb).play_sound(to_name(mrb, name), static_cast<float>(volume), static_cast<float>(pitch)));
}

mrb_value audio_music(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool loop = true;
  mrb_float volume = 0.0;
  mrb_get_args(mrb, "obf", &name, &loop, &volume);
  return mrb_bool_value(audio(mrb).play_music(to_name(mrb, name), loop, static_cast<float>(volume)));
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
  if (!ctx.scene_exists || !ctx.scene_exists(name)) mrb_raisef(mrb, E_ARGUMENT_ERROR, "no scene named %s", name.c_str());
  ctx.requested_scene = name;
  return mrb_nil_value();
}

mrb_value game_scene_name(mrb_state* mrb, mrb_value) { return mrb_str_new_cstr(mrb, context(mrb).scene_name.c_str()); }

// --- TestInput (system tests only) ---------------------------------------------------------------

ScriptedInputSource& test_input(mrb_state* mrb) {
  ScriptedInputSource* source = context(mrb).test_input;
  if (source == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "TestInput is only available when running a test");
  return *source;
}

mrb_value test_input_set(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool down = false;
  mrb_get_args(mrb, "ob", &name, &down);
  const std::string key_name = to_name(mrb, name);
  const auto key = key_from_name(key_name);
  if (!key) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown key :%s", key_name.c_str());
  test_input(mrb).set_key(*key, down);
  return mrb_nil_value();
}

mrb_value test_input_down(mrb_state* mrb, mrb_value) {
  const Key key = key_arg(mrb);
  return mrb_bool_value(test_input(mrb).is_key_down(key));
}

mrb_value test_input_release_all(mrb_state* mrb, mrb_value) {
  if (context(mrb).test_input != nullptr) context(mrb).test_input->release_all();
  return mrb_nil_value();
}

}  // namespace

void install_runtime_bindings(mrb_state* mrb) {
  RClass* input_module = mrb_define_module(mrb, "Input");
  mrb_define_module_function(mrb, input_module, "down?", input_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "pressed?", input_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "released?", input_released, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "axis", input_axis, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "action?", input_action, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "action_pressed?", input_action_pressed, MRB_ARGS_REQ(1));

  // mruby-time already defines the Time class; the game clock lives on it as class methods.
  RClass* time_class = mrb_define_class(mrb, "Time", mrb->object_class);
  mrb_define_class_method(mrb, time_class, "delta", time_delta, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "fixed_delta", time_fixed_delta, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "elapsed", time_elapsed, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "frame", time_frame, MRB_ARGS_NONE());

  RClass* ui_module = mrb_define_module(mrb, "UI");
  mrb_define_module_function(mrb, ui_module, "__text", ui_text, MRB_ARGS_REQ(8));

  RClass* debug_module = mrb_define_module(mrb, "Debug");
  mrb_define_module_function(mrb, debug_module, "log", debug_log, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, debug_module, "warn", debug_warn, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, debug_module, "error", debug_error, MRB_ARGS_REQ(1));

  RClass* game_module = mrb_define_module(mrb, "Game");
  mrb_define_module_function(mrb, game_module, "quit", game_quit, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game_module, "screenshot", game_screenshot, MRB_ARGS_REQ(1));

  mrb_define_module_function(mrb, game_module, "load_scene", game_load_scene, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, game_module, "scene_name", game_scene_name, MRB_ARGS_NONE());

  RClass* audio_module = mrb_define_module(mrb, "Audio");
  mrb_define_module_function(mrb, audio_module, "__play", audio_play, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, audio_module, "__music", audio_music, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, audio_module, "stop_music", audio_stop_music, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, audio_module, "volume=", audio_set_volume, MRB_ARGS_REQ(1));

  RClass* test_input_module = mrb_define_module(mrb, "TestInput");
  mrb_define_module_function(mrb, test_input_module, "set", test_input_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "release_all", test_input_release_all, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, test_input_module, "down?", test_input_down, MRB_ARGS_REQ(1));
}

}  // namespace redgem
