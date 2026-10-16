#pragma once

#include "input/input.hpp"
#include "input/touch_controls.hpp"

namespace redgem {

// The input of a touch screen: the devices it is given (a keyboard or a gamepad can be plugged
// into a phone) and, while no gamepad is plugged in, on-screen controls that stand in for one
// (see TouchControls). Chosen by the program that runs on a touch screen (app/android/android_main.cpp);
// elsewhere nobody makes one. The fingers are read through raylib, which requires an open window.
class TouchInputSource final : public InputSource {
 public:
  // `devices` has to outlive this.
  explicit TouchInputSource(const InputSource& devices) : devices_(devices) {}

  bool is_key_down(Key key) const override { return devices_.is_key_down(key); }
  MouseSample mouse() const override { return devices_.mouse(); }
  GamepadSample gamepad() const override;
  std::string typed_text() const override { return devices_.typed_text(); }
  const TouchLayout* touch_layout() const override { return shown_ ? &touch_.layout() : nullptr; }
  void show_touch_controls(bool show) override { wanted_ = show; }
  void fit_surface(float width, float height) override;

 private:
  const InputSource& devices_;
  // (Reading the gamepad is where the fingers are read: the layout is its by-product.)
  mutable TouchControls touch_;
  mutable bool shown_ = false;
  bool wanted_ = true;
  // The size of what the game is drawn on: the controls are laid out on it.
  float width_ = 0.0f;
  float height_ = 0.0f;
};

}  // namespace redgem
