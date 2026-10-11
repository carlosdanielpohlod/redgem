// Input natives: what a game reads from the keyboard, the mouse and a gamepad.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "input/keys.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

InputState& input(mrb_state* mrb) { return *context(mrb).input; }

// The key, gamepad axis or gamepad button a Ruby name stands for; an ArgumentError for an unknown one.
Key key_named(mrb_state* mrb, mrb_value name) {
  const std::string key_name = to_name(mrb, name);
  const auto key = key_from_name(key_name);
  if (!key) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown key :%s", key_name.c_str());
  return *key;
}

GamepadAxis gamepad_axis_named(mrb_state* mrb, mrb_value name) {
  const std::string axis_name = to_name(mrb, name);
  const auto axis = gamepad_axis_from_name(axis_name);
  if (!axis) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown gamepad axis :%s", axis_name.c_str());
  return *axis;
}

GamepadButton gamepad_button_named(mrb_state* mrb, mrb_value name) {
  const std::string button_name = to_name(mrb, name);
  const auto button = gamepad_button_from_name(button_name);
  if (!button) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown gamepad button :%s", button_name.c_str());
  return *button;
}

// The single argument of a native method, which names one of those.
mrb_value single_arg(mrb_state* mrb) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  return name;
}

Key key_arg(mrb_state* mrb) { return key_named(mrb, single_arg(mrb)); }
GamepadButton gamepad_button_arg(mrb_state* mrb) { return gamepad_button_named(mrb, single_arg(mrb)); }

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
  if (!button)
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown mouse button :%s (use :left, :right or :middle)", name.c_str());
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
mrb_value input_touch_controls(mrb_state* mrb, mrb_value) { return mrb_bool_value(context(mrb).touch_controls); }
mrb_value input_set_touch_controls(mrb_state* mrb, mrb_value) {
  mrb_bool show = true;
  mrb_get_args(mrb, "b", &show);
  context(mrb).touch_controls = show;
  return mrb_bool_value(show);
}

mrb_value input_text(mrb_state* mrb, mrb_value) { return text_value(mrb, input(mrb).typed_text()); }

mrb_value input_gamepad(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).gamepad_connected()); }
mrb_value input_gamepad_axis(mrb_state* mrb, mrb_value) {
  return mrb_float_value(mrb, input(mrb).gamepad_axis(gamepad_axis_named(mrb, single_arg(mrb))));
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

}  // namespace

namespace {

void install_key_bindings(RClass* input_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, input_module, "down?", input_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "pressed?", input_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "released?", input_released, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "axis", input_axis, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "action?", input_action, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "action_pressed?", input_action_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "text", input_text, MRB_ARGS_NONE());
}

void install_mouse_bindings(RClass* input_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, input_module, "mouse_x", input_mouse_x, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_y", input_mouse_y, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_dx", input_mouse_dx, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_dy", input_mouse_dy, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_wheel", input_mouse_wheel, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_down?", input_mouse_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "mouse_pressed?", input_mouse_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "mouse_released?", input_mouse_released, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "mouse_locked?", input_mouse_locked, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_locked=", input_set_mouse_locked, MRB_ARGS_REQ(1));
}

// Gamepads, and the touch controls that stand in for one on a phone.
void install_gamepad_bindings(RClass* input_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, input_module, "touch_controls?", input_touch_controls, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "touch_controls=", input_set_touch_controls, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad?", input_gamepad, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "gamepad_axis", input_gamepad_axis, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_down?", input_gamepad_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_pressed?", input_gamepad_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_released?", input_gamepad_released, MRB_ARGS_REQ(1));
}

}  // namespace

void install_input_bindings(mrb_state* mrb) {
  RClass* input_module = mrb_define_module(mrb, "Input");
  install_key_bindings(input_module, mrb);
  install_mouse_bindings(input_module, mrb);
  install_gamepad_bindings(input_module, mrb);
}

}  // namespace redgem
