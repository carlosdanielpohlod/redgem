#include <doctest/doctest.h>

#include "input/input.hpp"
#include "input/soft_keyboard.hpp"
#include "input/soft_keyboard_input_source.hpp"

using namespace redgem;

namespace {

constexpr TextInputRequest kNoField{};
constexpr TextInputRequest kField{true, false};
constexpr TextInputRequest kTappedField{true, true};
constexpr char32_t kLetterA = U'a';
constexpr char32_t kLetterB = U'b';
constexpr char32_t kLetterETilde = 0x1EBD;  // three bytes of UTF-8
constexpr const char* kTypedWithETilde = "xa\xE1\xBA\xBD";
constexpr char32_t kNewline = 0x0A;
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

// A frame of a game reading an input with a keyboard: what the text fields of the frame before
// asked is said, then the input is read.
void next_frame(SoftKeyboardInputSource& input, InputState& state) {
  input.request_text_input(kField);
  state.update(input);
}

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

TEST_CASE("a back key reaching the game leaves the keyboard down until a field is tapped") {
  // (The system's keyboard takes the back key while it is up: when one reaches the game, it is down.)
  CountingPlatform platform;
  SoftKeyboard keyboard(platform);
  keyboard.follow(kField);
  keyboard.back_pressed();
  CHECK_FALSE(keyboard.shown());
  keyboard.follow(kField);  // the field is still there, but the player put the keyboard away
  CHECK_FALSE(keyboard.shown());
  CHECK(platform.shows == 1);
  keyboard.follow(kTappedField);
  CHECK(keyboard.shown());
  CHECK(platform.shows == 2);
}

TEST_CASE("a back key with no keyboard up changes nothing") {
  CountingPlatform platform;
  SoftKeyboard keyboard(platform);
  keyboard.back_pressed();
  keyboard.follow(kField);
  CHECK(keyboard.shown());
  CHECK(platform.hides == 0);
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
  keyboard.back_pressed();
  keyboard.follow(kNoField);
  keyboard.follow(kField);
  CHECK(keyboard.shown());
}

TEST_CASE("the keyboard leaves the screen with the app and comes back with it") {
  CountingPlatform platform;
  SoftKeyboard keyboard(platform);
  keyboard.follow(kField);
  keyboard.leave_screen();
  CHECK_FALSE(keyboard.shown());
  CHECK(platform.hides == 1);
  keyboard.follow(kField);
  CHECK(keyboard.shown());
  CHECK(platform.shows == 2);
}

TEST_CASE("the keyboard goes when the game ends") {
  CountingPlatform platform;
  {
    SoftKeyboard keyboard(platform);
    keyboard.follow(kField);
  }
  CHECK(platform.hides == 1);
}

TEST_CASE("what the keyboard types joins what the devices typed, and is read once") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  devices.type_text("x");
  SoftKeyboardInputSource input(devices, platform);
  input.type(kLetterA);
  input.type(kLetterETilde);
  input.request_text_input(kField);
  CHECK(input.typed_text() == kTypedWithETilde);
  CHECK(input.typed_text() == "x");  // the devices' text is theirs: the scripted one types every frame
}

TEST_CASE("control characters and what is no letter are not typed") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  for (const char32_t code : {kNewline, kDelete, kSurrogate, kPastUnicode, kAccentWaiting}) input.type(code);
  input.request_text_input(kField);
  CHECK(input.typed_text().empty());
}

TEST_CASE("an editing key pressed and let go between two frames reads as pressed in the next") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  InputState state;
  next_frame(input, state);
  input.press(Key::Backspace);
  input.release(Key::Backspace);
  next_frame(input, state);
  CHECK(state.pressed(Key::Backspace));
  next_frame(input, state);
  CHECK(state.released(Key::Backspace));
}

TEST_CASE("an editing key held stays down until let go") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  InputState state;
  input.press(Key::Enter);
  next_frame(input, state);
  next_frame(input, state);
  CHECK(state.down(Key::Enter));
  input.release(Key::Enter);
  next_frame(input, state);
  CHECK_FALSE(state.down(Key::Enter));
}

TEST_CASE("keys hit between two frames each read as a press of their own, a frame apart") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  InputState state;
  input.hit(Key::Backspace);
  input.hit(Key::Backspace);
  next_frame(input, state);
  CHECK(state.pressed(Key::Backspace));
  next_frame(input, state);
  CHECK_FALSE(state.down(Key::Backspace));  // (let go, so that the next press is one)
  next_frame(input, state);
  CHECK(state.pressed(Key::Backspace));
  next_frame(input, state);
  next_frame(input, state);
  CHECK_FALSE(state.down(Key::Backspace));
}

TEST_CASE("letters and editing keys reach the frames in the order they were typed") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  InputState state;
  input.type(kLetterA);
  input.hit(Key::Backspace);
  input.type(kLetterB);
  next_frame(input, state);
  CHECK(state.typed_text() == "a");  // the letter Backspace takes off comes with it
  CHECK(state.pressed(Key::Backspace));
  next_frame(input, state);
  CHECK(state.typed_text() == "b");
  CHECK_FALSE(state.down(Key::Backspace));
}

TEST_CASE("the input with a keyboard follows the fields, starts the platform's frame, hands on the back key") {
  CountingPlatform platform;
  ScriptedInputSource devices;
  SoftKeyboardInputSource input(devices, platform);
  input.request_text_input(kField);
  CHECK(platform.frames == 1);
  CHECK(input.soft_keyboard_shown());
  input.back();
  CHECK_FALSE(input.soft_keyboard_shown());
  input.request_text_input(kTappedField);
  CHECK(input.soft_keyboard_shown());
  input.leave_screen();
  CHECK_FALSE(input.soft_keyboard_shown());
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
