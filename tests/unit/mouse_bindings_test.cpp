// Mouse buttons and the wheel bound to actions, and a capture catching them without taking a menu's
// clicks: the one that asked for the capture, and the release of the one it caught.

#include <doctest/doctest.h>

#include <optional>

#include "input/input.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kSecondPad = 1;
constexpr float kWheelUp = 1.0f;     // a notch of the wheel turned away from the player
constexpr float kWheelDown = -1.0f;  // and towards them

ActionBinding mouse_only(MouseInput input) { return {{}, {}, {input}}; }

}  // namespace

TEST_CASE("a mouse button triggers an action as a key does, the first player's alone") {
  ScriptedInputSource source;
  InputState input;
  input.bindings().declare_action("shoot", mouse_only(MouseInput::Left));
  source.set_mouse_button(MouseButton::Left, true);
  input.update(source);
  CHECK(input.action_pressed("shoot") == true);
  CHECK(input.action_down("shoot") == true);
  CHECK(input.action_down("shoot", kSecondPad) == false);  // (the mouse, like the keyboard, is the first player's)
  input.update(source);
  CHECK(input.action_pressed("shoot") == false);
  source.set_mouse_button(MouseButton::Left, false);
  input.update(source);
  CHECK(input.action_released("shoot") == true);
}

TEST_CASE("the side buttons bind as the others do") {
  ScriptedInputSource source;
  InputState input;
  input.bindings().declare_action("back", mouse_only(MouseInput::Side));
  input.bindings().declare_action("forward", mouse_only(MouseInput::Extra));
  source.set_mouse_button(MouseButton::Side, true);
  input.update(source);
  CHECK(input.mouse_pressed(MouseButton::Side));
  CHECK(input.action_pressed("back") == true);
  CHECK(input.action_down("forward") == false);
}

TEST_CASE("the wheel turned is a press on every frame it turns, and lets go on the first it does not") {
  ScriptedInputSource source;
  InputState input;
  input.bindings().declare_action("next_weapon", mouse_only(MouseInput::WheelUp));
  input.bindings().declare_action("previous_weapon", mouse_only(MouseInput::WheelDown));
  source.set_mouse_wheel(kWheelUp);
  input.update(source);
  CHECK(input.action_pressed("next_weapon") == true);
  CHECK(input.action_down("previous_weapon") == false);
  input.update(source);  // still turning: another press
  CHECK(input.action_pressed("next_weapon") == true);
  source.set_mouse_wheel(0.0f);
  input.update(source);
  CHECK(input.action_released("next_weapon") == true);
  CHECK(input.action_down("next_weapon") == false);
  source.set_mouse_wheel(kWheelDown);
  input.update(source);
  CHECK(input.action_pressed("previous_weapon") == true);
}

TEST_CASE("a side of an axis may be a mouse button") {
  ScriptedInputSource source;
  InputState input;
  input.bindings().declare_axis("turn",
                                {mouse_only(MouseInput::Left), mouse_only(MouseInput::Right), std::nullopt, false});
  source.set_mouse_button(MouseButton::Right, true);
  input.update(source);
  CHECK(input.axis("turn") == doctest::Approx(1.0f));
}

TEST_CASE("a capture catches a mouse button, not the click held when it was asked for") {
  ScriptedInputSource source;
  InputState input;
  source.set_mouse_button(MouseButton::Left, true);  // the click on a "press a key" cell, still held
  input.update(source);
  input.capture_next();
  input.update(source);
  CHECK_FALSE(input.take_captured().has_value());
  source.set_mouse_button(MouseButton::Left, false);
  input.update(source);
  CHECK_FALSE(input.take_captured().has_value());  // (nor its release)
  source.set_mouse_button(MouseButton::Right, true);
  input.update(source);
  const auto caught = input.take_captured();
  REQUIRE(caught.has_value());
  CHECK(caught->binding == InputBinding(MouseInput::Right));
  CHECK_FALSE(caught->pad.has_value());
}

TEST_CASE("the click a capture caught is neither pressed nor released for anything else") {
  ScriptedInputSource source;
  InputState input;
  input.capture_next();
  source.set_mouse_button(MouseButton::Left, true);
  input.update(source);
  CHECK(input.take_captured()->binding == InputBinding(MouseInput::Left));
  CHECK_FALSE(input.mouse_pressed(MouseButton::Left));
  CHECK_FALSE(input.mouse_down(MouseButton::Left));
  input.update(source);
  CHECK_FALSE(input.mouse_down(MouseButton::Left));  // still held: still the capture's
  source.set_mouse_button(MouseButton::Left, false);
  input.update(source);
  CHECK_FALSE(input.mouse_released(MouseButton::Left));  // (a button under the pointer is not clicked)
  source.set_mouse_button(MouseButton::Left, true);
  input.update(source);
  CHECK(input.mouse_pressed(MouseButton::Left));  // the next click is anybody's again
}

TEST_CASE("a capture catches the wheel's direction, and the turn is the capture's alone") {
  ScriptedInputSource source;
  InputState input;
  input.capture_next();
  source.set_mouse_wheel(kWheelDown);
  input.update(source);
  CHECK(input.take_captured()->binding == InputBinding(MouseInput::WheelDown));
  CHECK(input.mouse_wheel() == doctest::Approx(0.0f));
}

TEST_CASE("a capture told to leave the mouse out lets its clicks through") {
  ScriptedInputSource source;
  InputState input;
  input.capture_next(default_capture_cancels(), MouseCapture::Ignored);
  source.set_mouse_button(MouseButton::Left, true);
  source.set_mouse_wheel(kWheelUp);
  input.update(source);
  CHECK_FALSE(input.take_captured().has_value());
  CHECK(input.capturing());
  CHECK(input.mouse_pressed(MouseButton::Left));
  source.set_gamepad_button(GamepadButton::Y, true);
  input.update(source);
  CHECK(input.take_captured()->binding == InputBinding(GamepadButton::Y));
}
