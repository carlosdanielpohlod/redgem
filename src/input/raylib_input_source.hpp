#pragma once

#include "input/input.hpp"
#include "input/touch_controls.hpp"

namespace redgem {

// Reads the real keyboard, mouse and gamepad through raylib. Requires an open window. On a touch
// screen with no gamepad plugged in, on-screen controls stand in for one (see TouchControls).
class RaylibInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override;
  MouseSample mouse() const override;
  GamepadSample gamepad() const override;
  std::string typed_text() const override;
  const TouchLayout* touch_layout() const override;
  void show_touch_controls(bool show) override { touch_wanted_ = show; }

 private:
  // (Reading the gamepad is where the fingers are read: the layout is its by-product.)
  mutable TouchControls touch_;
  mutable bool touch_shown_ = false;
  bool touch_wanted_ = true;
};

}  // namespace redgem
