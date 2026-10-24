// Input natives: what a game reads from the keyboard, the mouse and a gamepad.

#include <mruby.h>
#include <mruby/array.h>

#include <cstddef>
#include <optional>
#include <string>

#include "input/keys.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr const char* kUnknownAction = "unknown action :%s";

InputState& input(mrb_state* mrb) { return *context(mrb).input; }

// The single argument of a native method, as what it names.
Key key_arg(mrb_state* mrb) {
  const std::string name = name_arg(mrb);
  return known(mrb, key_from_name(name), "unknown key :%s", name);
}

MouseButton mouse_button_arg(mrb_state* mrb) {
  const std::string name = name_arg(mrb);
  return known(mrb, mouse_button_from_name(name), "unknown mouse button :%s (use :left, :right or :middle)", name);
}

// --- Input ---------------------------------------------------------------------------------

mrb_value input_down(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).down(key_arg(mrb))); }
mrb_value input_pressed(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).pressed(key_arg(mrb))); }
mrb_value input_released(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).released(key_arg(mrb))); }

// A name and a gamepad's number, the arguments of what reads a player's actions and axes.
struct NameAndPad {
  std::string name;
  std::size_t pad = kFirstGamepad;
};

NameAndPad name_and_pad(mrb_state* mrb) {
  mrb_value name;
  mrb_int pad = 0;
  mrb_get_args(mrb, "oi", &name, &pad);
  return {to_name(mrb, name), pad_number(mrb, pad)};
}

mrb_value input_axis(mrb_state* mrb, mrb_value) {
  const NameAndPad args = name_and_pad(mrb);
  return mrb_float_value(mrb, known(mrb, input(mrb).axis(args.name, args.pad), "unknown axis :%s", args.name));
}

mrb_value input_action(mrb_state* mrb, mrb_value) {
  const NameAndPad args = name_and_pad(mrb);
  return mrb_bool_value(known(mrb, input(mrb).action_down(args.name, args.pad), kUnknownAction, args.name));
}

mrb_value input_action_pressed(mrb_state* mrb, mrb_value) {
  const NameAndPad args = name_and_pad(mrb);
  return mrb_bool_value(known(mrb, input(mrb).action_pressed(args.name, args.pad), kUnknownAction, args.name));
}

mrb_value input_action_released(mrb_state* mrb, mrb_value) {
  const NameAndPad args = name_and_pad(mrb);
  return mrb_bool_value(known(mrb, input(mrb).action_released(args.name, args.pad), kUnknownAction, args.name));
}

mrb_value input_mouse_x(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_x()); }
mrb_value input_mouse_y(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_y()); }
mrb_value input_mouse_dx(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_dx()); }
mrb_value input_mouse_dy(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_dy()); }
mrb_value input_mouse_wheel(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, input(mrb).mouse_wheel()); }
mrb_value input_mouse_down(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(input(mrb).mouse_down(mouse_button_arg(mrb)));
}
mrb_value input_mouse_pressed(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(input(mrb).mouse_pressed(mouse_button_arg(mrb)));
}
mrb_value input_mouse_released(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(input(mrb).mouse_released(mouse_button_arg(mrb)));
}
mrb_value input_touch_controls(mrb_state* mrb, mrb_value) { return mrb_bool_value(context(mrb).touch_controls); }
mrb_value input_set_touch_controls(mrb_state* mrb, mrb_value) {
  context(mrb).touch_controls = bool_arg(mrb);
  return mrb_bool_value(context(mrb).touch_controls);
}

mrb_value input_text(mrb_state* mrb, mrb_value) { return text_value(mrb, input(mrb).typed_text()); }

mrb_value input_gamepad(mrb_state* mrb, mrb_value) {
  mrb_int pad = 0;
  mrb_get_args(mrb, "i", &pad);
  return mrb_bool_value(input(mrb).gamepad_connected(pad_number(mrb, pad)));
}
mrb_value input_gamepad_axis(mrb_state* mrb, mrb_value) {
  const NameAndPad args = name_and_pad(mrb);
  const GamepadAxis axis = known(mrb, gamepad_axis_from_name(args.name), "unknown gamepad axis :%s", args.name);
  return mrb_float_value(mrb, input(mrb).gamepad_axis(axis, args.pad));
}

// A gamepad button and the gamepad it is on.
struct ButtonOnPad {
  GamepadButton button;
  std::size_t pad;
};

ButtonOnPad button_on_pad(mrb_state* mrb) {
  const NameAndPad args = name_and_pad(mrb);
  return {known(mrb, gamepad_button_from_name(args.name), "unknown gamepad button :%s", args.name), args.pad};
}

mrb_value input_gamepad_down(mrb_state* mrb, mrb_value) {
  const ButtonOnPad args = button_on_pad(mrb);
  return mrb_bool_value(input(mrb).gamepad_down(args.button, args.pad));
}
mrb_value input_gamepad_pressed(mrb_state* mrb, mrb_value) {
  const ButtonOnPad args = button_on_pad(mrb);
  return mrb_bool_value(input(mrb).gamepad_pressed(args.button, args.pad));
}
mrb_value input_gamepad_released(mrb_state* mrb, mrb_value) {
  const ButtonOnPad args = button_on_pad(mrb);
  return mrb_bool_value(input(mrb).gamepad_released(args.button, args.pad));
}

mrb_value input_pads(mrb_state* mrb, mrb_value) {
  const mrb_value pads = mrb_ary_new(mrb);
  for (const std::size_t pad : input(mrb).connected_pads()) {
    mrb_ary_push(mrb, pads, mrb_int_value(mrb, static_cast<mrb_int>(pad)));
  }
  return pads;
}

mrb_value input_mouse_locked(mrb_state* mrb, mrb_value) { return mrb_bool_value(context(mrb).mouse_locked); }
mrb_value input_set_mouse_locked(mrb_state* mrb, mrb_value) {
  context(mrb).mouse_locked = bool_arg(mrb);
  return mrb_bool_value(context(mrb).mouse_locked);
}

void install_key_bindings(RClass* input_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, input_module, "down?", input_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "pressed?", input_pressed, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "released?", input_released, MRB_ARGS_REQ(1));
  // (Input.axis(name, pad: 0) and the others that take a player are in the prelude: ruby/engine/input.rb.)
  mrb_define_module_function(mrb, input_module, "__axis", input_axis, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__action_down", input_action, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__action_pressed", input_action_pressed, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__action_released", input_action_released, MRB_ARGS_REQ(2));
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
  mrb_define_module_function(mrb, input_module, "__gamepad", input_gamepad, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, input_module, "__gamepad_axis", input_gamepad_axis, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__gamepad_down", input_gamepad_down, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__gamepad_pressed", input_gamepad_pressed, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__gamepad_released", input_gamepad_released, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "pads", input_pads, MRB_ARGS_NONE());
}

}  // namespace

void install_input_bindings(mrb_state* mrb) {
  RClass* input_module = mrb_define_module(mrb, "Input");
  install_key_bindings(input_module, mrb);
  install_mouse_bindings(input_module, mrb);
  install_gamepad_bindings(input_module, mrb);
  install_input_action_bindings(mrb);  // the game's own actions, and the players' bindings of them
}

}  // namespace redgem
