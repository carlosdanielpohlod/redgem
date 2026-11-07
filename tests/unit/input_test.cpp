#include <doctest/doctest.h>

#include <set>
#include <string>

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
  CHECK(mouse_button_from_name("side") == MouseButton::Side);
  CHECK(mouse_button_from_name("extra") == MouseButton::Extra);
  CHECK_FALSE(mouse_button_from_name("thumb").has_value());
  CHECK_FALSE(mouse_button_from_name("wheel_up").has_value());  // (the wheel is no button)
  CHECK(mouse_input_from_name("wheel_up") == MouseInput::WheelUp);
  CHECK(mouse_input_from_name("extra") == MouseInput::Extra);
  CHECK(mouse_input_name(MouseInput::WheelDown) == "wheel_down");
  CHECK_FALSE(mouse_input_from_name("wheel_left").has_value());
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
  CHECK_FALSE(key_from_name("f0").has_value());
  CHECK_FALSE(key_from_name("f13").has_value());
  CHECK_FALSE(key_from_name("kp_10").has_value());
  constexpr std::size_t kLetters = 26;
  constexpr std::size_t kDigits = 10;
  constexpr std::size_t kFunctionKeys = 12;
  constexpr std::size_t kKeypadDigits = 10;
  // Space and punctuation (12), editing keys (10), arrows (4), locks and system keys (6),
  // modifiers (8), the keypad's other keys (7).
  constexpr std::size_t kNamedKeys = 47;
  CHECK(all_keys().size() == kLetters + kDigits + kFunctionKeys + kKeypadDigits + kNamedKeys);
}

TEST_CASE("the function keys, the editing block, the keypad and the rest of a full keyboard have names") {
  CHECK(key_from_name("f1") == Key::F1);
  CHECK(key_from_name("f5") == Key::F5);
  CHECK(key_from_name("f12") == Key::F12);
  CHECK(key_from_name("insert") == Key::Insert);
  CHECK(key_from_name("delete") == Key::Delete);
  CHECK(key_from_name("home") == Key::Home);
  CHECK(key_from_name("end") == Key::End);
  CHECK(key_from_name("page_up") == Key::PageUp);
  CHECK(key_from_name("page_down") == Key::PageDown);
  CHECK(key_from_name("kp_0") == Key::Keypad0);
  CHECK(key_from_name("kp_9") == Key::Keypad9);
  CHECK(key_from_name("kp_enter") == Key::KeypadEnter);
  CHECK(key_from_name("kp_add") == Key::KeypadAdd);
  CHECK(key_from_name("right_alt") == Key::RightAlt);
  CHECK(key_from_name("caps_lock") == Key::CapsLock);
  CHECK(key_from_name("left_bracket") == Key::LeftBracket);
  CHECK(key_from_name("right_bracket") == Key::RightBracket);
  CHECK(key_from_name("semicolon") == Key::Semicolon);
  CHECK(key_from_name("equals") == Key::Equals);
  CHECK(key_from_name("backslash") == Key::Backslash);
  CHECK(key_from_name("grave") == Key::Grave);
}

TEST_CASE("every key goes by one name, which reads back as that key") {
  std::set<std::string> names;
  for (const Key key : all_keys()) {
    const std::string name = key_name(key);
    CHECK(key_from_name(name) == key);
    names.insert(name);
  }
  CHECK(names.size() == all_keys().size());
  CHECK(key_name(Key::F10) == "f10");
  CHECK(key_name(Key::Keypad3) == "kp_3");
}

TEST_CASE("a new key is read as any other") {
  ScriptedInputSource source;
  InputState input;
  source.set_key(Key::F5, true);
  source.set_key(Key::Keypad1, true);
  input.update(source);
  CHECK(input.pressed(Key::F5));
  CHECK(input.down(Key::Keypad1));
  source.set_key(Key::F5, false);
  input.update(source);
  CHECK(input.released(Key::F5));
}
