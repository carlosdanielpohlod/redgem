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

  TouchLayout layout_;
  int stick_touch_ = kNoTouch;  // the finger on the stick, while one is
  float base_x_ = 0.0f;         // where it landed
  float base_y_ = 0.0f;
  float last_x_ = 0.0f;         // where it was a frame ago
  float last_y_ = 0.0f;
};

}  // namespace redgem
