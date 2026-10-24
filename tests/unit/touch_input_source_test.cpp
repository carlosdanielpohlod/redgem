#include <doctest/doctest.h>

#include "input/touch_input_source.hpp"

using namespace redgem;

namespace {

// A game drawn in a part of a window: smaller than any window it could sit in.
constexpr float kViewWidth = 800.0f;
constexpr float kViewHeight = 400.0f;
constexpr float kSmallWidth = 200.0f;
constexpr float kSmallHeight = 100.0f;
constexpr float kHalf = 0.5f;

}  // namespace

TEST_CASE("a touch screen with no gamepad plugged in shows controls that stand in for one") {
  const ScriptedInputSource devices;
  TouchInputSource screen(devices);
  screen.fit_surface(kViewWidth, kViewHeight);
  const GamepadSample pad = screen.gamepad(kFirstGamepad);
  CHECK(pad.connected);
  REQUIRE(screen.touch_layout() != nullptr);
}

TEST_CASE("the touch controls are laid out on what the game is drawn on, not on the window") {
  const ScriptedInputSource devices;
  TouchInputSource screen(devices);
  screen.fit_surface(kViewWidth, kViewHeight);
  screen.gamepad(kFirstGamepad);
  const TouchLayout large = *screen.touch_layout();
  CHECK(large.a.x > kViewWidth * kHalf);
  CHECK(large.a.x < kViewWidth);
  CHECK(large.a.y < kViewHeight);
  // The same game in a smaller view: every circle is inside it.
  screen.fit_surface(kSmallWidth, kSmallHeight);
  screen.gamepad(kFirstGamepad);
  const TouchLayout small = *screen.touch_layout();
  for (const TouchCircle& circle : {small.stick_base, small.a, small.b, small.start}) {
    CHECK(circle.x < kSmallWidth);
    CHECK(circle.y < kSmallHeight);
  }
  CHECK(small.a.radius < large.a.radius);
}

TEST_CASE("a plugged-in gamepad is read instead of the screen, and its controls are not shown") {
  ScriptedInputSource devices;
  devices.set_gamepad_button(GamepadButton::B, true);
  TouchInputSource screen(devices);
  screen.fit_surface(kViewWidth, kViewHeight);
  const GamepadSample pad = screen.gamepad(kFirstGamepad);
  CHECK(pad.connected);
  CHECK(pad.buttons[static_cast<std::size_t>(GamepadButton::B)]);
  CHECK(screen.touch_layout() == nullptr);
}

TEST_CASE("a game with a touch interface of its own hides the controls, and no gamepad stands in") {
  const ScriptedInputSource devices;
  TouchInputSource screen(devices);
  screen.fit_surface(kViewWidth, kViewHeight);
  screen.show_touch_controls(false);
  CHECK_FALSE(screen.gamepad(kFirstGamepad).connected);
  CHECK(screen.touch_layout() == nullptr);
  screen.show_touch_controls(true);
  CHECK(screen.gamepad(kFirstGamepad).connected);
  CHECK(screen.touch_layout() != nullptr);
}

TEST_CASE("keys, the mouse and typed text come from the devices behind a touch screen") {
  constexpr float kMouseX = 12.0f;
  constexpr float kMouseY = 34.0f;
  ScriptedInputSource devices;
  devices.set_key(Key::W, true);
  devices.move_mouse(kMouseX, kMouseY);
  devices.type_text("hi");
  const TouchInputSource screen(devices);
  CHECK(screen.is_key_down(Key::W));
  CHECK_FALSE(screen.is_key_down(Key::S));
  CHECK(screen.mouse().x == doctest::Approx(kMouseX));
  CHECK(screen.mouse().y == doctest::Approx(kMouseY));
  CHECK(screen.typed_text() == "hi");
}
