// Input, Time, UI, Debug, Game and TestInput natives.

#include <cmath>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "app/save_store.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "input/keys.hpp"
#include "scene/animation_player.hpp"
#include "scene/particles.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr float kMaxColorChannel = 255.0f;
constexpr double kLargestSavedInteger = 9007199254740992.0;  // 2^53: whole numbers a double holds exactly

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

MouseButton button_arg(mrb_state* mrb) {
  const std::string name = name_arg(mrb);
  const auto button = mouse_button_from_name(name);
  if (!button) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown mouse button :%s (use :left, :right or :middle)", name.c_str());
  return *button;
}

mrb_value input_mouse_x(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_x()); }
mrb_value input_mouse_y(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_y()); }
mrb_value input_mouse_dx(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_dx()); }
mrb_value input_mouse_dy(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_dy()); }
mrb_value input_mouse_wheel(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_wheel()); }
mrb_value input_mouse_down(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).mouse_down(button_arg(mrb))); }
mrb_value input_mouse_pressed(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(input(mrb).mouse_pressed(button_arg(mrb)));
}
mrb_value input_mouse_released(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(input(mrb).mouse_released(button_arg(mrb)));
}
GamepadButton gamepad_button_arg(mrb_state* mrb) {
  const std::string name = name_arg(mrb);
  const auto button = gamepad_button_from_name(name);
  if (!button) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown gamepad button :%s", name.c_str());
  return *button;
}

mrb_value input_gamepad(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).gamepad_connected()); }
mrb_value input_gamepad_axis(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  const auto axis = gamepad_axis_from_name(name);
  if (!axis) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown gamepad axis :%s", name.c_str());
  return mrb_float_value(mrb, input(mrb).gamepad_axis(*axis));
}
mrb_value input_gamepad_down(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(input(mrb).gamepad_down(gamepad_button_arg(mrb)));
}
mrb_value input_gamepad_pressed(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(input(mrb).gamepad_pressed(gamepad_button_arg(mrb)));
}
mrb_value input_gamepad_released(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(input(mrb).gamepad_released(gamepad_button_arg(mrb)));
}

mrb_value input_mouse_locked(mrb_state* mrb, mrb_value) { return mrb_bool_value(context(mrb).mouse_locked); }
mrb_value input_set_mouse_locked(mrb_state* mrb, mrb_value) {
  mrb_bool locked = false;
  mrb_get_args(mrb, "b", &locked);
  context(mrb).mouse_locked = locked;
  return mrb_bool_value(locked);
}

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

Color color_from(mrb_int r, mrb_int g, mrb_int b, mrb_int a) {
  return {static_cast<float>(r) / kMaxColorChannel, static_cast<float>(g) / kMaxColorChannel,
          static_cast<float>(b) / kMaxColorChannel, static_cast<float>(a) / kMaxColorChannel};
}

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
  item.color = color_from(static_cast<mrb_int>(kMaxColorChannel), static_cast<mrb_int>(kMaxColorChannel),
                          static_cast<mrb_int>(kMaxColorChannel), a);
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
  return mrb_bool_value(audio(mrb).play_sound(to_name(mrb, name), static_cast<float>(volume), static_cast<float>(pitch)));
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
  if (!ctx.scene_exists || !ctx.scene_exists(name)) mrb_raisef(mrb, E_ARGUMENT_ERROR, "no scene named %s", name.c_str());
  ctx.requested_scene = name;
  return mrb_nil_value();
}

mrb_value game_scene_name(mrb_state* mrb, mrb_value) { return mrb_str_new_cstr(mrb, context(mrb).scene_name.c_str()); }

// --- Save --------------------------------------------------------------------------------------

SaveStore& save(mrb_state* mrb) {
  SaveStore* store = context(mrb).save;
  if (store == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "Save is only available while a game runs");
  return *store;
}

mrb_value save_get(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value fallback = mrb_nil_value();
  mrb_get_args(mrb, "o|o", &key, &fallback);
  const auto value = save(mrb).get(to_name(mrb, key));
  if (!value) return fallback;
  if (const auto* flag = std::get_if<bool>(&*value)) return mrb_bool_value(*flag);
  if (const auto* number = std::get_if<double>(&*value)) {
    // Whole numbers come back as Integers: a saved count of 3 is 3, not 3.0.
    if (std::fabs(*number) > kLargestSavedInteger || std::floor(*number) != *number) return mrb_float_value(mrb, *number);
    return mrb_int_value(mrb, static_cast<mrb_int>(*number));
  }
  const auto& text = std::get<std::string>(*value);
  return mrb_str_new(mrb, text.data(), static_cast<mrb_int>(text.size()));
}

mrb_value save_set(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "oo", &key, &value);
  PropertyValue stored;
  if (mrb_true_p(value) || mrb_false_p(value)) {
    stored = mrb_true_p(value);
  } else if (mrb_integer_p(value)) {
    stored = static_cast<double>(mrb_integer(value));
  } else if (mrb_float_p(value)) {
    if (!std::isfinite(mrb_float(value))) mrb_raise(mrb, E_ARGUMENT_ERROR, "Save cannot keep infinity or NaN");
    stored = static_cast<double>(mrb_float(value));
  } else if (mrb_string_p(value) || mrb_symbol_p(value)) {
    stored = to_name(mrb, value);
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Save keeps numbers, texts and true / false");
  }
  if (!save(mrb).set(to_name(mrb, key), stored)) log::warning("could not write " + save(mrb).file().string());
  return value;
}

mrb_value save_delete(mrb_state* mrb, mrb_value) {
  save(mrb).remove(name_arg(mrb));
  return mrb_nil_value();
}

mrb_value save_clear(mrb_state* mrb, mrb_value) {
  save(mrb).clear();
  return mrb_nil_value();
}

mrb_value save_keys(mrb_state* mrb, mrb_value) {
  mrb_value names = mrb_ary_new(mrb);
  for (const std::string& key : save(mrb).keys()) {
    mrb_ary_push(mrb, names, mrb_symbol_value(mrb_intern_cstr(mrb, key.c_str())));
  }
  return names;
}

// --- Animation ---------------------------------------------------------------------------------

AnimationPlayer& animations(mrb_state* mrb) {
  AnimationPlayer* player = context(mrb).animations;
  if (player == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "Animation is only available while a game runs");
  return *player;
}

// The clip's name, which must be one of the scene's.
std::string clip_arg(mrb_state* mrb, mrb_value name) {
  const std::string clip = to_name(mrb, name);
  if (!animations(mrb).has(clip)) mrb_raisef(mrb, E_ARGUMENT_ERROR, "the scene has no animation called '%s'", clip.c_str());
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

// --- Particles ---------------------------------------------------------------------------------

ParticleSystem& particles(mrb_state* mrb) {
  ParticleSystem* system = context(mrb).particles;
  if (system == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "Particles are only available while a game runs");
  return *system;
}

mrb_value particles_burst(mrb_state* mrb, mrb_value) {
  mrb_value position, direction;
  mrb_int count = 0, r = 0, g = 0, b = 0, a = 0;
  mrb_float speed = 0.0, spread = 0.0, life = 0.0, size = 0.0, gravity = 0.0;
  mrb_bool glow = false;
  mrb_get_args(mrb, "ooifffffiiiib", &position, &direction, &count, &speed, &spread, &life, &size, &gravity, &r, &g,
               &b, &a, &glow);
  ParticleBurst burst;
  burst.position = to_vec3(mrb, position);
  burst.direction = to_vec3(mrb, direction);
  burst.count = static_cast<int>(count);
  burst.speed = static_cast<float>(speed);
  burst.spread_degrees = static_cast<float>(spread);
  burst.life = static_cast<float>(life);
  burst.size = static_cast<float>(size);
  burst.gravity = static_cast<float>(gravity);
  burst.color = color_from(r, g, b, a);
  burst.glow = glow;
  return mrb_int_value(mrb, particles(mrb).burst(burst));
}

mrb_value particles_count(mrb_state* mrb, mrb_value) {
  return mrb_int_value(mrb, static_cast<mrb_int>(particles(mrb).particles().size()));
}

mrb_value particles_clear(mrb_state* mrb, mrb_value) {
  particles(mrb).clear();
  return mrb_nil_value();
}

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

mrb_value test_input_move_mouse(mrb_state* mrb, mrb_value) {
  mrb_float x = 0.0, y = 0.0;
  mrb_get_args(mrb, "ff", &x, &y);
  test_input(mrb).move_mouse(static_cast<float>(x), static_cast<float>(y));
  return mrb_nil_value();
}

mrb_value test_input_set_mouse_button(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool down = false;
  mrb_get_args(mrb, "ob", &name, &down);
  const std::string button_name = to_name(mrb, name);
  const auto button = mouse_button_from_name(button_name);
  if (!button) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown mouse button :%s", button_name.c_str());
  test_input(mrb).set_mouse_button(*button, down);
  return mrb_nil_value();
}

mrb_value test_input_set_mouse_wheel(mrb_state* mrb, mrb_value) {
  mrb_float notches = 0.0;
  mrb_get_args(mrb, "f", &notches);
  test_input(mrb).set_mouse_wheel(static_cast<float>(notches));
  return mrb_nil_value();
}

mrb_value test_input_set_gamepad_axis(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_float value = 0.0;
  mrb_get_args(mrb, "of", &name, &value);
  const std::string axis_name = to_name(mrb, name);
  const auto axis = gamepad_axis_from_name(axis_name);
  if (!axis) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown gamepad axis :%s", axis_name.c_str());
  test_input(mrb).set_gamepad_axis(*axis, static_cast<float>(value));
  return mrb_nil_value();
}

mrb_value test_input_set_gamepad_button(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool down = false;
  mrb_get_args(mrb, "ob", &name, &down);
  const std::string button_name = to_name(mrb, name);
  const auto button = gamepad_button_from_name(button_name);
  if (!button) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown gamepad button :%s", button_name.c_str());
  test_input(mrb).set_gamepad_button(*button, down);
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
  mrb_define_module_function(mrb, input_module, "mouse_x", input_mouse_x, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_y", input_mouse_y, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_dx", input_mouse_dx, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_dy", input_mouse_dy, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_wheel", input_mouse_wheel, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_down?", input_mouse_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "mouse_pressed?", input_mouse_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "mouse_released?", input_mouse_released, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad?", input_gamepad, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "gamepad_axis", input_gamepad_axis, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_down?", input_gamepad_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_pressed?", input_gamepad_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_released?", input_gamepad_released, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "mouse_locked?", input_mouse_locked, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_locked=", input_set_mouse_locked, MRB_ARGS_REQ(1));

  // mruby-time already defines the Time class; the game clock lives on it as class methods.
  RClass* time_class = mrb_define_class(mrb, "Time", mrb->object_class);
  mrb_define_class_method(mrb, time_class, "delta", time_delta, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "fixed_delta", time_fixed_delta, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "elapsed", time_elapsed, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "frame", time_frame, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "scale", time_scale, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "scale=", time_set_scale, MRB_ARGS_REQ(1));

  RClass* ui_module = mrb_define_module(mrb, "UI");
  mrb_define_module_function(mrb, ui_module, "__text", ui_text, MRB_ARGS_REQ(8));
  mrb_define_module_function(mrb, ui_module, "__rect", ui_rect, MRB_ARGS_REQ(9));
  mrb_define_module_function(mrb, ui_module, "__image", ui_image, MRB_ARGS_REQ(6));
  mrb_define_module_function(mrb, ui_module, "__text_width", ui_text_width, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, ui_module, "width", ui_width, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "height", ui_height, MRB_ARGS_NONE());

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
  mrb_define_module_function(mrb, audio_module, "music_pitch=", audio_set_music_pitch, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "music_volume=", audio_set_music_volume, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, audio_module, "volume=", audio_set_volume, MRB_ARGS_REQ(1));

  RClass* animation_module = mrb_define_module(mrb, "Animation");
  mrb_define_module_function(mrb, animation_module, "names", animation_names, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, animation_module, "__play", animation_play, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, animation_module, "pause", animation_pause, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, animation_module, "stop", animation_stop, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, animation_module, "seek", animation_seek, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, animation_module, "playing?", animation_playing, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, animation_module, "time", animation_time, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, animation_module, "duration", animation_duration, MRB_ARGS_REQ(1));

  RClass* particles_module = mrb_define_module(mrb, "Particles");
  mrb_define_module_function(mrb, particles_module, "__burst", particles_burst, MRB_ARGS_REQ(13));
  mrb_define_module_function(mrb, particles_module, "count", particles_count, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, particles_module, "clear", particles_clear, MRB_ARGS_NONE());

  RClass* save_module = mrb_define_module(mrb, "Save");
  mrb_define_module_function(mrb, save_module, "get", save_get, MRB_ARGS_ARG(1, 1));
  mrb_define_module_function(mrb, save_module, "[]", save_get, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, save_module, "set", save_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, save_module, "[]=", save_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, save_module, "delete", save_delete, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, save_module, "clear", save_clear, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, save_module, "keys", save_keys, MRB_ARGS_NONE());

  RClass* test_input_module = mrb_define_module(mrb, "TestInput");
  mrb_define_module_function(mrb, test_input_module, "set", test_input_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "release_all", test_input_release_all, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, test_input_module, "down?", test_input_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, test_input_module, "set_gamepad_axis", test_input_set_gamepad_axis, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "set_gamepad_button", test_input_set_gamepad_button, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "move_mouse", test_input_move_mouse, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "set_mouse_button", test_input_set_mouse_button, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "set_mouse_wheel", test_input_set_mouse_wheel, MRB_ARGS_REQ(1));
}

}  // namespace redgem
