#pragma once

#include <string>

#include "input/raylib_input_source.hpp"

namespace redgem {

// Keyboard and mouse reach the game only while the Scene view has focus during play. The game
// sees the mouse in the view's own pixels, as if the view were its window.
class GatedInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override { return enabled && device.is_key_down(key); }
  MouseSample mouse() const override {
    MouseSample sample = device.mouse();
    sample.x -= origin_x;
    sample.y -= origin_y;
    if (!enabled) sample.buttons = {}, sample.wheel = 0.0f;
    return sample;
  }
  GamepadSample gamepad(std::size_t pad) const override { return enabled ? device.gamepad(pad) : GamepadSample{}; }
  std::string typed_text() const override { return enabled ? device.typed_text() : std::string(); }
  bool enabled = false;
  float origin_x = 0.0f;  // where the Scene view's image starts in the window
  float origin_y = 0.0f;
  RaylibInputSource device;
};

}  // namespace redgem
