#include <doctest/doctest.h>

#include "input/touch_controls.hpp"

using namespace redgem;

namespace {

constexpr float kWidth = 1600.0f;
constexpr float kHeight = 720.0f;
constexpr int kThumb = 7;        // ids of the fingers
constexpr int kOtherFinger = 9;
constexpr float kLanding = 300.0f;   // where the left thumb lands, both ways
constexpr float kFarPush = 500.0f;   // pixels: much further than the stick reaches
constexpr float kRightSide = 1200.0f;

float axis(const GamepadSample& pad, GamepadAxis which) { return pad.axes[static_cast<std::size_t>(which)]; }
bool button(const GamepadSample& pad, GamepadButton which) { return pad.buttons[static_cast<std::size_t>(which)]; }

}  // namespace

TEST_CASE("with no finger down the touch gamepad is there, at rest") {
  TouchControls controls;
  const GamepadSample pad = controls.update({}, kWidth, kHeight);
  CHECK(pad.connected);
  CHECK(axis(pad, GamepadAxis::LeftX) == doctest::Approx(0.0f));
  CHECK_FALSE(button(pad, GamepadButton::A));
  CHECK_FALSE(controls.layout().stick_base.pressed);
  CHECK(controls.layout().a.x > kWidth / 2.0f);          // the buttons are on the right
  CHECK(controls.layout().stick_base.x < kWidth / 2.0f); // the stick rests on the left
}

TEST_CASE("the stick starts where the left thumb lands and follows it, up to a full push") {
  TouchControls controls;
  GamepadSample pad = controls.update({{kThumb, kLanding, kLanding}}, kWidth, kHeight);
  CHECK(axis(pad, GamepadAxis::LeftX) == doctest::Approx(0.0f));  // just landed
  CHECK(controls.layout().stick_base.x == doctest::Approx(kLanding));
  const float reach = controls.layout().stick_base.radius;

  pad = controls.update({{kThumb, kLanding + reach / 2.0f, kLanding}}, kWidth, kHeight);
  CHECK(axis(pad, GamepadAxis::LeftX) == doctest::Approx(0.5f));
  CHECK(axis(pad, GamepadAxis::LeftY) == doctest::Approx(0.0f));
  pad = controls.update({{kThumb, kLanding, kLanding - kFarPush}}, kWidth, kHeight);
  CHECK(axis(pad, GamepadAxis::LeftY) == doctest::Approx(-1.0f));  // up, and no more than all the way
  CHECK(controls.layout().stick_knob.y == doctest::Approx(kLanding - reach));
  // The thumb may wander anywhere once it is on the stick, even over the buttons.
  pad = controls.update({{kThumb, controls.layout().a.x, controls.layout().a.y}}, kWidth, kHeight);
  CHECK(axis(pad, GamepadAxis::LeftX) == doctest::Approx(1.0f).epsilon(0.05));
  CHECK_FALSE(button(pad, GamepadButton::A));

  pad = controls.update({}, kWidth, kHeight);  // let go
  CHECK(axis(pad, GamepadAxis::LeftX) == doctest::Approx(0.0f));
  CHECK_FALSE(controls.layout().stick_base.pressed);
}

TEST_CASE("buttons answer to fingers on them, while another holds the stick") {
  TouchControls controls;
  controls.update({}, kWidth, kHeight);
  const TouchLayout at = controls.layout();
  GamepadSample pad = controls.update({{kThumb, kLanding, kLanding}, {kOtherFinger, at.a.x, at.a.y}}, kWidth, kHeight);
  CHECK(button(pad, GamepadButton::A));
  CHECK_FALSE(button(pad, GamepadButton::B));
  CHECK(controls.layout().a.pressed);
  pad = controls.update({{kThumb, kLanding + kFarPush, kLanding}, {kOtherFinger, at.b.x, at.b.y}}, kWidth, kHeight);
  CHECK(button(pad, GamepadButton::B));
  CHECK(axis(pad, GamepadAxis::LeftX) == doctest::Approx(1.0f));
  pad = controls.update({{kOtherFinger, at.start.x, at.start.y}}, kWidth, kHeight);
  CHECK(button(pad, GamepadButton::Start));
  // A finger on the right that is on no button does nothing (it is not a second stick).
  pad = controls.update({{kOtherFinger, kRightSide, kLanding}}, kWidth, kHeight);
  CHECK_FALSE(button(pad, GamepadButton::A));
  CHECK(axis(pad, GamepadAxis::LeftX) == doctest::Approx(0.0f));
}

TEST_CASE("the controls keep their proportions on any screen") {
  TouchControls small, large;
  small.update({}, kWidth, kHeight);
  large.update({}, kWidth * 2.0f, kHeight * 2.0f);
  CHECK(large.layout().a.radius == doctest::Approx(small.layout().a.radius * 2.0f));
  CHECK(large.layout().a.x == doctest::Approx(small.layout().a.x * 2.0f));
}
