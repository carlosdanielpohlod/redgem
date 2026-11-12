#include <doctest/doctest.h>

#include "input/input.hpp"
#include "input/soft_keyboard.hpp"
#include "input/soft_keyboard_input_source.hpp"

using namespace redgem;

namespace {

constexpr TextInputRequest kNoField{};
constexpr TextInputRequest kField{true, false};
constexpr TextInputRequest kTappedField{true, true};
constexpr bool kDown = true;
constexpr bool kUp = false;
constexpr char32_t kLetterA = U'a';
constexpr char32_t kLetterETilde = 0x1EBD;  // three bytes of UTF-8
constexpr char32_t kNewline = U'\n';
constexpr char32_t kDelete = 0x7F;
constexpr char32_t kSurrogate = 0xD800;
constexpr char32_t kPastUnicode = 0x110000;
constexpr char32_t kAccentWaiting = 0x80000000;  // what Java gives for a dead key: a letter yet to come

// A keyboard that only counts what it was asked to do.
struct CountingPlatform final : SoftKeyboardPlatform {
  int shows = 0;
  int hides = 0;
  int frames = 0;
  void show() override { ++shows; }
  void hide() override { ++hides; }
  void begin_frame() override { ++frames; }
};

}  // namespace

TEST_CASE("the keyboard comes up while a text field is drawn and goes when none is") {
  CountingPlatform platform;
  SoftKeyboard keyboard(platform);
  keyboard.follow(kNoField);
  CHECK(platform.hides == 0);  // (it was never up)
  keyboard.follow(kField);
  keyboard.follow(kField);
  CHECK(keyboard.shown());
  CHECK(platform.shows == 1);  // once, not every frame
  keyboard.follow(kNoField);   // the field closed, or the scene changed
  CHECK_FALSE(keyboard.shown());
  CHECK(platform.hides == 1);
}

TEST_CASE("the back key puts the keyboard away, and the game does not see it") {
  CountingPlatform platform;
  SoftKeyboard keyboard(platform);
  keyboard.follow(kField);
  CHECK(keyboard.take_back(kDown));
  CHECK_FALSE(keyboard.shown());
  CHECK(keyboard.take_back(kDown));  // held: repeats of the same press
  CHECK(keyboard.take_back(kUp));
  keyboard.follow(kField);  // the field is still there, but the player put the keyboard away
  CHECK_FALSE(keyboard.shown());
  CHECK(platform.shows == 1);
  CHECK_FALSE(keyboard.take_back(kDown));  // with no keyboard up, the back key is the game's
  CHECK_FALSE(keyboard.take_back(kUp));
}

TEST_CASE("tapping a field brings back the keyboard the player put away") {
  CountingPlatform platform;
  SoftKeyboard keyboard(platform);
  keyboard.follow(kField);
  keyboard.take_back(kDown);
  keyboard.take_back(kUp);
  keyboard.follow(kTappedField);
  CHECK(keyboard.shown());
  CHECK(platform.shows == 2);
}

TEST_CASE("a tap shows the keyboard again even when it should be up already") {
  // (The system's own back key may have closed it without the game hearing of it.)
  CountingPlatform platform;
  SoftKeyboard keyboard(platform);
  keyboard.follow(kField);
  keyboard.follow(kTappedField);
  CHECK(platform.shows == 2);
}

TEST_CASE("a field drawn again after none was starts with the keyboard up, even if it was put away") {
  CountingPlatform platform;
  SoftKeyboard keyboard(platform);
  keyboard.follow(kField);
  keyboard.take_back(kDown);
  keyboard.take_back(kUp);
  keyboard.follow(kNoField);
  keyboard.follow(kField);
  CHECK(keyboard.shown());
}

TEST_CASE("what the keyboard types joins what the devices typed, and is read once") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  devices.type_text("x");
  SoftKeyboardInputSource input(devices, platform);
  input.type(kLetterA);
  input.type(kLetterETilde);
  CHECK(input.typed_text() == "xa\xE1\xBA\xBD");
  CHECK(input.typed_text() == "x");  // the devices' text is theirs: the scripted one types every frame
}

TEST_CASE("control characters and what is no letter are not typed") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  for (const char32_t code : {kNewline, kDelete, kSurrogate, kPastUnicode, kAccentWaiting}) input.type(code);
  CHECK(input.typed_text().empty());
}

TEST_CASE("an editing key pressed and let go between two frames reads as pressed in the next") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  InputState state;
  input.request_text_input(kField);
  state.update(input);
  input.press(Key::Backspace);
  input.release(Key::Backspace);
  input.request_text_input(kField);
  state.update(input);
  CHECK(state.pressed(Key::Backspace));
  input.request_text_input(kField);
  state.update(input);
  CHECK(state.released(Key::Backspace));
}

TEST_CASE("an editing key held stays down, and one hit reads as pressed once") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  input.press(Key::Enter);
  input.request_text_input(kNoField);
  input.request_text_input(kNoField);
  CHECK(input.is_key_down(Key::Enter));
  input.release(Key::Enter);
  input.hit(Key::Backspace);
  input.request_text_input(kNoField);
  CHECK_FALSE(input.is_key_down(Key::Enter));
  CHECK(input.is_key_down(Key::Backspace));
  input.request_text_input(kNoField);
  CHECK_FALSE(input.is_key_down(Key::Backspace));
}

TEST_CASE(
    "the input with a keyboard follows the text fields, lets the platform start its frame, and hands the back key "
    "over") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  input.request_text_input(kField);
  CHECK(platform.frames == 1);
  CHECK(input.soft_keyboard_shown());
  CHECK(input.back(kDown));
  CHECK(input.back(kUp));
  CHECK_FALSE(input.soft_keyboard_shown());
  CHECK_FALSE(input.back(kDown));
}

TEST_CASE("the input with a keyboard is the devices' for everything else") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  devices.set_key(Key::A, true);
  devices.set_touch_pointer(true);
  devices.set_gamepad_button(GamepadButton::A, true);
  SoftKeyboardInputSource input(devices, platform);
  CHECK(input.is_key_down(Key::A));
  CHECK(input.touch_pointer());
  CHECK(input.gamepad(kFirstGamepad).connected);
  CHECK(input.touch_layout() == nullptr);
}
