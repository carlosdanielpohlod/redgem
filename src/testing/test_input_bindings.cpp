// TestInput: what a test presses, moves and types.

#include "testing/test_input_bindings.hpp"

#include <mruby/variable.h>

#include <string>

#include "input/keys.hpp"
#include "script/bindings/binding_helpers.hpp"

namespace redgem::testing {
namespace {

using binding::known;
using binding::to_name;

// Where the VM keeps the pointer to the source its TestInput drives: a variable of the module
// itself, so it lives exactly as long as the VM and two VMs in one program never share one.
// Its name does not start with @, which Ruby code can neither read nor write: only these natives
// reach it.
constexpr const char* kSourceVariable = "__driven_input";

Key key_named(mrb_state* mrb, mrb_value name) {
  const std::string key_name = to_name(mrb, name);
  return known(mrb, key_from_name(key_name), "unknown key :%s", key_name);
}

mrb_value set_key(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool down = false;
  mrb_get_args(mrb, "ob", &name, &down);
  const Key key = key_named(mrb, name);
  driven_input(mrb).set_key(key, down);
  return mrb_nil_value();
}

mrb_value key_down(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  const Key key = key_named(mrb, name);
  return mrb_bool_value(driven_input(mrb).is_key_down(key));
}

mrb_value release_all(mrb_state* mrb, mrb_value) {
  driven_input(mrb).release_all();
  return mrb_nil_value();
}

mrb_value type_text(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_get_args(mrb, "z", &text);
  driven_input(mrb).type_text(text);
  return mrb_nil_value();
}

mrb_value move_mouse(mrb_state* mrb, mrb_value) {
  mrb_float x = 0.0, y = 0.0;
  mrb_get_args(mrb, "ff", &x, &y);
  driven_input(mrb).move_mouse(static_cast<float>(x), static_cast<float>(y));
  return mrb_nil_value();
}

mrb_value set_mouse_button(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool down = false;
  mrb_get_args(mrb, "ob", &name, &down);
  const std::string button_name = to_name(mrb, name);
  const MouseButton button = known(mrb, mouse_button_from_name(button_name), "unknown mouse button :%s", button_name);
  driven_input(mrb).set_mouse_button(button, down);
  return mrb_nil_value();
}

mrb_value set_mouse_wheel(mrb_state* mrb, mrb_value) {
  mrb_float notches = 0.0;
  mrb_get_args(mrb, "f", &notches);
  driven_input(mrb).set_mouse_wheel(static_cast<float>(notches));
  return mrb_nil_value();
}

mrb_value set_gamepad_axis(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_float value = 0.0;
  mrb_get_args(mrb, "of", &name, &value);
  const std::string axis_name = to_name(mrb, name);
  const GamepadAxis axis = known(mrb, gamepad_axis_from_name(axis_name), "unknown gamepad axis :%s", axis_name);
  driven_input(mrb).set_gamepad_axis(axis, static_cast<float>(value));
  return mrb_nil_value();
}

mrb_value set_gamepad_button(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_bool down = false;
  mrb_get_args(mrb, "ob", &name, &down);
  const std::string button_name = to_name(mrb, name);
  const GamepadButton button =
      known(mrb, gamepad_button_from_name(button_name), "unknown gamepad button :%s", button_name);
  driven_input(mrb).set_gamepad_button(button, down);
  return mrb_nil_value();
}

void define_keyboard(mrb_state* mrb, RClass* module) {
  mrb_define_module_function(mrb, module, "set", set_key, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, module, "down?", key_down, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, module, "release_all", release_all, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, module, "type", type_text, MRB_ARGS_REQ(1));
}

void define_mouse_and_gamepad(mrb_state* mrb, RClass* module) {
  mrb_define_module_function(mrb, module, "move_mouse", move_mouse, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, module, "set_mouse_button", set_mouse_button, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, module, "set_mouse_wheel", set_mouse_wheel, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, module, "set_gamepad_axis", set_gamepad_axis, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, module, "set_gamepad_button", set_gamepad_button, MRB_ARGS_REQ(2));
}

}  // namespace

ScriptedInputSource& driven_input(mrb_state* mrb) {
  const mrb_value module = mrb_obj_value(mrb_module_get(mrb, kTestInputModule));
  const mrb_value kept = mrb_iv_get(mrb, module, mrb_intern_cstr(mrb, kSourceVariable));
  const bool has_source = mrb_cptr_p(kept);
  if (!has_source) mrb_raise(mrb, E_RUNTIME_ERROR, "TestInput has no input to drive");
  return *static_cast<ScriptedInputSource*>(mrb_cptr(kept));
}

void install_test_input(mrb_state* mrb, ScriptedInputSource& source) {
  RClass* module = mrb_define_module(mrb, kTestInputModule);
  mrb_iv_set(mrb, mrb_obj_value(module), mrb_intern_cstr(mrb, kSourceVariable), mrb_cptr_value(mrb, &source));
  define_keyboard(mrb, module);
  define_mouse_and_gamepad(mrb, module);
  install_test_notes(mrb, module);
}

}  // namespace redgem::testing
