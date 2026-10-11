#include <doctest/doctest.h>

#include "input/input.hpp"

using namespace redgem;

namespace {
constexpr float kStickHalfway = 0.5f;
constexpr float kStickDrift = 0.1f;  // inside the dead zone
}  // namespace

TEST_CASE("key names map to keys") {
  CHECK(key_from_name("a") == Key::A);
  CHECK(key_from_name("z") == Key::Z);
  CHECK(key_from_name("7") == Key::Seven);
  CHECK(key_from_name("space") == Key::Space);
  CHECK(key_from_name("left_shift") == Key::LeftShift);
  CHECK_FALSE(key_from_name("hyper").has_value());
}

TEST_CASE("pressed and released are edge triggered") {
  ScriptedInputSource source;
  InputState input;
  source.set_key(Key::Space, true);
  input.update(source);
  CHECK(input.down(Key::Space));
  CHECK(input.pressed(Key::Space));
  input.update(source);
  CHECK(input.down(Key::Space));
  CHECK_FALSE(input.pressed(Key::Space));
  source.set_key(Key::Space, false);
  input.update(source);
  CHECK(input.released(Key::Space));
  CHECK_FALSE(input.down(Key::Space));
}

TEST_CASE("axes combine their keys") {
  ScriptedInputSource source;
  InputState input;
  input.update(source);
  CHECK(input.axis("horizontal") == doctest::Approx(0.0f));
  source.set_key(Key::D, true);
  input.update(source);
  CHECK(input.axis("horizontal") == doctest::Approx(1.0f));
  source.set_key(Key::Left, true);
  input.update(source);
  CHECK(input.axis("horizontal") == doctest::Approx(0.0f));
  source.release_all();
  source.set_key(Key::W, true);
  input.update(source);
  CHECK(input.axis("vertical") == doctest::Approx(1.0f));
  CHECK_FALSE(input.axis("sideways").has_value());
}

TEST_CASE("actions") {
  ScriptedInputSource source;
  InputState input;
  source.set_key(Key::Space, true);
  input.update(source);
  CHECK(input.action_down("jump") == true);
  CHECK(input.action_pressed("jump") == true);
  CHECK_FALSE(input.action_down("dance").has_value());
}

TEST_CASE("a gamepad's stick moves the axes, by as much as it is pushed") {
  ScriptedInputSource source;
  InputState input;
  input.update(source);
  CHECK_FALSE(input.gamepad_connected());
  source.set_gamepad_axis(GamepadAxis::LeftX, kStickHalfway);
  source.set_gamepad_axis(GamepadAxis::LeftY, -1.0f);  // pushed away from the player
  input.update(source);
  CHECK(input.gamepad_connected());
  CHECK(input.axis("horizontal") == doctest::Approx(kStickHalfway));
  CHECK(input.axis("vertical") == doctest::Approx(1.0f));  // forward
  // A key wins over the stick.
  source.set_key(Key::A, true);
  input.update(source);
  CHECK(input.axis("horizontal") == doctest::Approx(-1.0f));
}

TEST_CASE("a stick resting slightly off center reads as centered; triggers do not") {
  ScriptedInputSource source;
  InputState input;
  source.set_gamepad_axis(GamepadAxis::LeftX, kStickDrift);
  source.set_gamepad_axis(GamepadAxis::RightTrigger, kStickDrift);
  input.update(source);
  CHECK(input.gamepad_axis(GamepadAxis::LeftX) == doctest::Approx(0.0f));
  CHECK(input.axis("horizontal") == doctest::Approx(0.0f));
  CHECK(input.gamepad_axis(GamepadAxis::RightTrigger) == doctest::Approx(kStickDrift));
}

TEST_CASE("gamepad buttons trigger actions and can be read on their own") {
  ScriptedInputSource source;
  InputState input;
  source.set_gamepad_button(GamepadButton::A, true);
  input.update(source);
  CHECK(input.action_down("jump") == true);
  CHECK(input.action_pressed("jump") == true);
  CHECK(input.gamepad_down(GamepadButton::A));
  CHECK(input.gamepad_pressed(GamepadButton::A));
  CHECK_FALSE(input.gamepad_down(GamepadButton::B));
  input.update(source);
  CHECK(input.action_pressed("jump") == false);  // still held
  CHECK_FALSE(input.gamepad_pressed(GamepadButton::A));
  source.set_gamepad_button(GamepadButton::A, false);
  input.update(source);
  CHECK(input.gamepad_released(GamepadButton::A));
  CHECK(input.action_down("jump") == false);
  source.set_gamepad_button(GamepadButton::Start, true);
  source.release_all();  // unplugs it too
  input.update(source);
  CHECK_FALSE(input.gamepad_connected());
}

TEST_CASE("gamepad axes and buttons have names") {
  CHECK(gamepad_axis_from_name("left_x") == GamepadAxis::LeftX);
  CHECK(gamepad_axis_from_name("right_trigger") == GamepadAxis::RightTrigger);
  CHECK_FALSE(gamepad_axis_from_name("middle").has_value());
  CHECK(gamepad_button_from_name("a") == GamepadButton::A);
  CHECK(gamepad_button_from_name("dpad_left") == GamepadButton::DpadLeft);
  CHECK(gamepad_button_from_name("right_stick") == GamepadButton::RightStick);
  CHECK_FALSE(gamepad_button_from_name("turbo").has_value());
}

TEST_CASE("mouse buttons have names") {
  CHECK(mouse_button_from_name("left") == MouseButton::Left);
  CHECK(mouse_button_from_name("right") == MouseButton::Right);
  CHECK(mouse_button_from_name("middle") == MouseButton::Middle);
  CHECK_FALSE(mouse_button_from_name("thumb").has_value());
}

TEST_CASE("every letter, digit and named key is known, and nothing else") {
  CHECK(key_from_name("a") == Key::A);
  CHECK(key_from_name("z") == Key::Z);
  CHECK(key_from_name("0") == Key::Zero);
  CHECK(key_from_name("9") == Key::Nine);
  CHECK(key_from_name("slash") == Key::Slash);
  CHECK(key_from_name("backspace") == Key::Backspace);
  CHECK(key_from_name("up") == Key::Up);
  CHECK(key_from_name("right_control") == Key::RightControl);
  CHECK_FALSE(key_from_name("A").has_value());
  CHECK_FALSE(key_from_name("f1").has_value());
  constexpr std::size_t kLetters = 26;
  constexpr std::size_t kDigits = 10;
  constexpr std::size_t kNamedKeys = 19;  // space and punctuation, editing keys, arrows, modifiers
  CHECK(all_keys().size() == kLetters + kDigits + kNamedKeys);
}
