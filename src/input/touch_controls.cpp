#include "input/touch_controls.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {

namespace {

// Everything is sized by the screen's height, so it fits a thumb on any phone held sideways.
constexpr float kStickZone = 0.45f;         // share of the screen's width, from the left, where a thumb starts the stick
constexpr float kStickRadius = 0.17f;       // of the height: how far the thumb travels for a full push
constexpr float kKnobRadius = 0.075f;
constexpr float kRestingStickX = 0.24f;     // of the height: where the stick is drawn while nobody holds it
constexpr float kRestingStickFromBottom = 0.27f;
constexpr float kButtonRadius = 0.09f;
constexpr float kSmallButtonRadius = 0.05f;
constexpr float kButtonReach = 1.3f;        // a button answers to touches this many radii from its middle
constexpr float kAFromRight = 0.16f;        // A: the lower, outer button
constexpr float kAFromBottom = 0.2f;
constexpr float kBFromRight = 0.37f;        // B: left of it and a little lower
constexpr float kBFromBottom = 0.13f;
constexpr float kStartFromRight = 0.09f;    // Start: small, in the top corner
constexpr float kStartFromTop = 0.09f;

bool touching(const TouchCircle& button, const TouchPoint& touch) {
  return std::hypot(touch.x - button.x, touch.y - button.y) <= button.radius * kButtonReach;
}

}  // namespace

GamepadSample TouchControls::update(const std::vector<TouchPoint>& touches, float width, float height) {
  layout_.a = {width - kAFromRight * height, height - kAFromBottom * height, kButtonRadius * height, false};
  layout_.b = {width - kBFromRight * height, height - kBFromBottom * height, kButtonRadius * height, false};
  layout_.start = {width - kStartFromRight * height, kStartFromTop * height, kSmallButtonRadius * height, false};

  // The finger on the stick: the one that was, if still down; else one newly down on the left.
  const auto held = std::find_if(touches.begin(), touches.end(),
                                 [this](const TouchPoint& touch) { return touch.id == stick_touch_; });
  const TouchPoint* thumb = held != touches.end() ? &*held : nullptr;
  if (thumb == nullptr) {
    stick_touch_ = kNoTouch;
    for (const TouchPoint& touch : touches) {
      if (touch.x > width * kStickZone) continue;
      stick_touch_ = touch.id;
      base_x_ = touch.x;
      base_y_ = touch.y;
      thumb = &touch;
      break;
    }
  }

  GamepadSample pad;
  pad.connected = true;
  const float reach = kStickRadius * height;
  float push_x = 0.0f;
  float push_y = 0.0f;
  if (thumb != nullptr && reach > 0.0f) {
    push_x = (thumb->x - base_x_) / reach;
    push_y = (thumb->y - base_y_) / reach;
    const float length = std::hypot(push_x, push_y);
    if (length > 1.0f) {  // the knob stays within the stick's reach
      push_x /= length;
      push_y /= length;
    }
  }
  pad.axes[static_cast<std::size_t>(GamepadAxis::LeftX)] = push_x;
  pad.axes[static_cast<std::size_t>(GamepadAxis::LeftY)] = push_y;
  const float rest_x = thumb != nullptr ? base_x_ : kRestingStickX * height;
  const float rest_y = thumb != nullptr ? base_y_ : height - kRestingStickFromBottom * height;
  layout_.stick_base = {rest_x, rest_y, reach, thumb != nullptr};
  layout_.stick_knob = {rest_x + push_x * reach, rest_y + push_y * reach, kKnobRadius * height, thumb != nullptr};

  for (const TouchPoint& touch : touches) {
    if (touch.id == stick_touch_) continue;
    layout_.a.pressed = layout_.a.pressed || touching(layout_.a, touch);
    layout_.b.pressed = layout_.b.pressed || touching(layout_.b, touch);
    layout_.start.pressed = layout_.start.pressed || touching(layout_.start, touch);
  }
  pad.buttons[static_cast<std::size_t>(GamepadButton::A)] = layout_.a.pressed;
  pad.buttons[static_cast<std::size_t>(GamepadButton::B)] = layout_.b.pressed;
  pad.buttons[static_cast<std::size_t>(GamepadButton::Start)] = layout_.start.pressed;
  return pad;
}

}  // namespace redgem
