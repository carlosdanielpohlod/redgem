#include "input/touch_controls.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {

namespace {

// Everything is sized by the screen's height, so it fits a thumb on any phone held sideways.
constexpr float kStickZone = 0.45f;    // share of the screen's width, from the left, where a thumb starts the stick
constexpr float kStickRadius = 0.17f;  // of the height: how far the thumb travels for a full push
constexpr float kKnobRadius = 0.075f;
constexpr float kRestingStickX = 0.24f;  // of the height: where the stick is drawn while nobody holds it
constexpr float kRestingStickFromBottom = 0.27f;
constexpr float kButtonRadius = 0.09f;
constexpr float kSmallButtonRadius = 0.05f;
constexpr float kButtonReach = 1.3f;  // a button answers to touches this many radii from its middle
constexpr float kAFromRight = 0.16f;  // A: the lower, outer button
constexpr float kAFromBottom = 0.2f;
constexpr float kBFromRight = 0.37f;  // B: left of it and a little lower
constexpr float kBFromBottom = 0.13f;
constexpr float kStartFromRight = 0.09f;  // Start: small, in the top corner
constexpr float kStartFromTop = 0.09f;
constexpr float kTeleport = 0.6f;  // of the height: no thumb moves this far between two frames

bool touching(const TouchCircle& button, const TouchPoint& touch) {
  return std::hypot(touch.x - button.x, touch.y - button.y) <= button.radius * kButtonReach;
}

// The left part of the screen, where a thumb coming down starts the stick.
bool in_stick_zone(const TouchPoint& touch, float width) { return !(touch.x > width * kStickZone); }

}  // namespace

StickPush stick_push(float dx, float dy, float reach) {
  if (reach <= 0.0f) return {};
  StickPush push{dx / reach, dy / reach};
  const float length = std::hypot(push.x, push.y);
  if (length > 1.0f) {  // the knob stays within the stick's reach
    push.x /= length;
    push.y /= length;
  }
  return push;
}

void TouchControls::place_buttons(float width, float height) {
  layout_.a = {width - kAFromRight * height, height - kAFromBottom * height, kButtonRadius * height, false};
  layout_.b = {width - kBFromRight * height, height - kBFromBottom * height, kButtonRadius * height, false};
  layout_.start = {width - kStartFromRight * height, kStartFromTop * height, kSmallButtonRadius * height, false};
}

const TouchPoint* TouchControls::held_thumb(const std::vector<TouchPoint>& touches, float height) const {
  const auto held = std::find_if(touches.begin(), touches.end(),
                                 [this](const TouchPoint& touch) { return touch.id == stick_touch_; });
  if (held == touches.end()) return nullptr;
  // A finger that crossed the screen in one frame is another finger with a reused number: the
  // thumb lifted, and someone pressed a button.
  const bool teleported = std::hypot(held->x - last_x_, held->y - last_y_) > kTeleport * height;
  return teleported ? nullptr : &*held;
}

const TouchPoint* TouchControls::landed_thumb(const std::vector<TouchPoint>& touches, float width) {
  stick_touch_ = kNoTouch;
  const auto landed = std::find_if(touches.begin(), touches.end(),
                                   [width](const TouchPoint& touch) { return in_stick_zone(touch, width); });
  if (landed == touches.end()) return nullptr;
  stick_touch_ = landed->id;
  base_x_ = landed->x;
  base_y_ = landed->y;
  return &*landed;
}

const TouchPoint* TouchControls::find_thumb(const std::vector<TouchPoint>& touches, float width, float height) {
  // The finger on the stick: the one that was, if still down; else one newly down on the left.
  const TouchPoint* thumb = held_thumb(touches, height);
  if (thumb == nullptr) thumb = landed_thumb(touches, width);
  if (thumb != nullptr) {
    last_x_ = thumb->x;
    last_y_ = thumb->y;
  }
  return thumb;
}

void TouchControls::place_stick(bool held, StickPush push, float height) {
  const float reach = kStickRadius * height;
  const float rest_x = held ? base_x_ : kRestingStickX * height;
  const float rest_y = held ? base_y_ : height - kRestingStickFromBottom * height;
  layout_.stick_base = {rest_x, rest_y, reach, held};
  layout_.stick_knob = {rest_x + push.x * reach, rest_y + push.y * reach, kKnobRadius * height, held};
}

void TouchControls::press_buttons(const std::vector<TouchPoint>& touches) {
  for (const TouchPoint& touch : touches) {
    if (touch.id == stick_touch_) continue;
    layout_.a.pressed = layout_.a.pressed || touching(layout_.a, touch);
    layout_.b.pressed = layout_.b.pressed || touching(layout_.b, touch);
    layout_.start.pressed = layout_.start.pressed || touching(layout_.start, touch);
  }
}

GamepadSample TouchControls::as_gamepad(StickPush push) const {
  GamepadSample pad;
  pad.connected = true;
  pad.axes[static_cast<std::size_t>(GamepadAxis::LeftX)] = push.x;
  pad.axes[static_cast<std::size_t>(GamepadAxis::LeftY)] = push.y;
  pad.buttons[static_cast<std::size_t>(GamepadButton::A)] = layout_.a.pressed;
  pad.buttons[static_cast<std::size_t>(GamepadButton::B)] = layout_.b.pressed;
  pad.buttons[static_cast<std::size_t>(GamepadButton::Start)] = layout_.start.pressed;
  return pad;
}

GamepadSample TouchControls::update(const std::vector<TouchPoint>& touches, float width, float height) {
  place_buttons(width, height);
  const TouchPoint* thumb = find_thumb(touches, width, height);
  StickPush push;
  if (thumb != nullptr) push = stick_push(thumb->x - base_x_, thumb->y - base_y_, kStickRadius * height);
  place_stick(thumb != nullptr, push, height);
  press_buttons(touches);
  return as_gamepad(push);
}

}  // namespace redgem
