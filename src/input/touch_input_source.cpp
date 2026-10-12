#include "input/touch_input_source.hpp"

#include <raylib.h>

#include <vector>

namespace redgem {

namespace {

std::vector<TouchPoint> read_touches() {
  std::vector<TouchPoint> touches;
  for (int i = 0; i < GetTouchPointCount(); ++i) {
    const Vector2 at = GetTouchPosition(i);
    touches.push_back({GetTouchPointId(i), at.x, at.y});
  }
  return touches;
}

}  // namespace

void TouchInputSource::fit_surface(float width, float height) {
  width_ = width;
  height_ = height;
}

GamepadSample TouchInputSource::gamepad() const {
  const GamepadSample plugged = devices_.gamepad();
  shown_ = wanted_ && !plugged.connected;
  return shown_ ? touch_.update(read_touches(), width_, height_) : plugged;
}

}  // namespace redgem
