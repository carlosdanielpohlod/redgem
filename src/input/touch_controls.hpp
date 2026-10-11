#pragma once

#include <vector>

#include "input/input.hpp"

namespace redgem {

// A finger on the screen, in pixels from the top-left corner. `id` stays the same while it is down.
struct TouchPoint {
  int id = 0;
  float x = 0.0f;
  float y = 0.0f;
};

// One of the on-screen controls, as a circle to draw.
struct TouchCircle {
  float x = 0.0f;
  float y = 0.0f;
  float radius = 0.0f;
  bool pressed = false;
};

// Where the on-screen controls are this frame.
struct TouchLayout {
  TouchCircle stick_base;  // where the thumb landed (or where it usually does, while it is up)
  TouchCircle stick_knob;  // where the thumb is now
  TouchCircle a;
  TouchCircle b;
  TouchCircle start;
};

// How far a stick is pushed, each way from -1 to 1 (left and up are negative).
struct StickPush {
  float x = 0.0f;
  float y = 0.0f;
};
// The push of a thumb `dx`, `dy` pixels from where it landed, on a stick it can move `reach`
// pixels each way: never more than 1 in all, however far the thumb strays.
StickPush stick_push(float dx, float dy, float reach);

// The controls of a touch screen: a stick under the left thumb, wherever it lands on the left
// part of the screen, and buttons under the right one. Together they act as a gamepad (the left
// stick, A, B and Start), so a game that works with a gamepad works on a phone as it is.
class TouchControls {
 public:
  // The gamepad the fingers on a screen of that size amount to.
  GamepadSample update(const std::vector<TouchPoint>& touches, float width, float height);
  const TouchLayout& layout() const { return layout_; }

 private:
  static constexpr int kNoTouch = -1;

  void place_buttons(float width, float height);
  // The finger that was on the stick, if it is still down.
  const TouchPoint* held_thumb(const std::vector<TouchPoint>& touches, float height) const;
  // A finger newly down on the left part of the screen, which takes the stick; null when none is.
  const TouchPoint* landed_thumb(const std::vector<TouchPoint>& touches, float width);
  const TouchPoint* find_thumb(const std::vector<TouchPoint>& touches, float width, float height);
  void place_stick(bool held, StickPush push, float height);
  void press_buttons(const std::vector<TouchPoint>& touches);
  GamepadSample as_gamepad(StickPush push) const;

  TouchLayout layout_;
  int stick_touch_ = kNoTouch;  // the finger on the stick, while one is
  float base_x_ = 0.0f;         // where it landed
  float base_y_ = 0.0f;
  float last_x_ = 0.0f;  // where it was a frame ago
  float last_y_ = 0.0f;
};

}  // namespace redgem
