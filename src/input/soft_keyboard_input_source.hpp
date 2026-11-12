#pragma once

#include <set>
#include <string>

#include "input/input_source.hpp"
#include "input/soft_keyboard.hpp"

namespace redgem {

// A touch screen's input with its on-screen keyboard in front (see SoftKeyboard for when it is up).
// The platform hands on its key events as they come: the letters they type join the text typed
// (raylib turns none of a phone's key events into letters), the editing keys (Backspace, Enter)
// read as held for at least the frame they went down in (a phone's keyboard lets go of them in the
// same instant, before any frame could see them), and the back key closes the keyboard while it
// is up. Everything else is the input in front of which it is put. Chosen by the program for a
// phone (app/android/android_main.cpp); elsewhere nobody makes one.
class SoftKeyboardInputSource final : public InputSource {
 public:
  // `devices` and `platform` have to outlive this.
  SoftKeyboardInputSource(InputSource& devices, SoftKeyboardPlatform& platform)
      : devices_(devices), platform_(platform), keyboard_(platform) {}

  bool is_key_down(Key key) const override { return devices_.is_key_down(key) || holds(key); }
  MouseSample mouse() const override { return devices_.mouse(); }
  bool touch_pointer() const override { return devices_.touch_pointer(); }
  GamepadSample gamepad(std::size_t pad) const override { return devices_.gamepad(pad); }
  // What the devices typed, then what the keyboard did since the last frame read it.
  std::string typed_text() const override;
  const TouchLayout* touch_layout() const override { return devices_.touch_layout(); }
  void show_touch_controls(bool show) override { devices_.show_touch_controls(show); }
  void fit_surface(float width, float height) override { devices_.fit_surface(width, height); }
  void request_text_input(const TextInputRequest& request) override;
  bool soft_keyboard_shown() const override { return keyboard_.shown(); }

  // A letter typed, as a Unicode code point: control characters and what is no letter at all are
  // left out.
  void type(char32_t letter);
  // An editing key going down, coming up, or both at once (a key event that only says it was hit).
  void press(Key key);
  void release(Key key) { held_.erase(key); }
  void hit(Key key);
  // The back key going down (or up): true when it was the keyboard's, which the game must not see.
  bool back(bool down) { return keyboard_.take_back(down); }

 private:
  // Whether the keyboard holds a key: it is down, or went down since the frame before started.
  bool holds(Key key) const { return held_.count(key) != 0 || this_frame_.count(key) != 0; }

  InputSource& devices_;
  SoftKeyboardPlatform& platform_;
  SoftKeyboard keyboard_;
  // (Reading the text typed takes it: the next frame reads only what came after.)
  mutable std::string typed_;
  std::set<Key> held_;
  std::set<Key> since_frame_;  // the keys that went down since this frame started
  std::set<Key> this_frame_;   // those that went down before it, which it reads as held
};

}  // namespace redgem
