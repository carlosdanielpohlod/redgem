// Input natives, and TestInput: what a system test presses, moves and types.

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

Key key_arg(mrb_state* mrb) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  const std::string key_name = to_name(mrb, name);
  const auto key = key_from_name(key_name);
  if (!key) mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown key :%s", key_name.c_str());
  return *key;
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

mrb_value input_touch_controls(mrb_state* mrb, mrb_value) { return mrb_bool_value(context(mrb).touch_controls); }
mrb_value input_set_touch_controls(mrb_state* mrb, mrb_value) {
  mrb_bool show = true;
  mrb_get_args(mrb, "b", &show);
  context(mrb).touch_controls = show;
  return mrb_bool_value(show);
}

mrb_value input_text(mrb_state* mrb, mrb_value) {
  const std::string& typed = input(mrb).typed_text();
  return mrb_str_new(mrb, typed.data(), static_cast<mrb_int>(typed.size()));
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

mrb_value test_input_type(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_get_args(mrb, "z", &text);
  test_input(mrb).type_text(text);
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

void install_input_bindings(mrb_state* mrb) {
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
  mrb_define_module_function(mrb, input_module, "text", input_text, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "touch_controls?", input_touch_controls, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "touch_controls=", input_set_touch_controls, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad?", input_gamepad, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "gamepad_axis", input_gamepad_axis, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_down?", input_gamepad_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_pressed?", input_gamepad_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "gamepad_released?", input_gamepad_released, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "mouse_locked?", input_mouse_locked, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "mouse_locked=", input_set_mouse_locked, MRB_ARGS_REQ(1));

  RClass* test_input_module = mrb_define_module(mrb, "TestInput");
  mrb_define_module_function(mrb, test_input_module, "set", test_input_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "release_all", test_input_release_all, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, test_input_module, "down?", test_input_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, test_input_module, "type", test_input_type, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, test_input_module, "set_gamepad_axis", test_input_set_gamepad_axis, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "set_gamepad_button", test_input_set_gamepad_button, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "move_mouse", test_input_move_mouse, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "set_mouse_button", test_input_set_mouse_button, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "set_mouse_wheel", test_input_set_mouse_wheel, MRB_ARGS_REQ(1));
}

}  // namespace redgem
