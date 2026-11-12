#pragma once

#include <deque>
#include <optional>
#include <set>
#include <string>

#include "input/input_source.hpp"
#include "input/soft_keyboard.hpp"

namespace redgem {

// A touch screen's input with its on-screen keyboard in front (see SoftKeyboard for when it is up).
// The platform hands on its key events as they come: the letters they type join the text typed
// (raylib turns none of a phone's key events into letters), and the editing keys (Backspace,
// Enter) read as pressed in a frame of their own, even when let go in the same instant (as a
// phone's keyboard does, before any frame could see them held). What was typed reaches the frames
// in the order it was typed: a frame takes the letters up to the next editing key, and that key
// unless the frame before took one (a key pressed in two frames in a row would read as held). The
// back key and the app leaving the screen go to the keyboard. Everything else is the input in
// front of which it is put. Chosen by the program for a phone (app/android/android_main.cpp);
// elsewhere nobody makes one.
class SoftKeyboardInputSource final : public InputSource {
 public:
  // `devices` and `platform` have to outlive this.
  SoftKeyboardInputSource(InputSource& devices, SoftKeyboardPlatform& platform)
      : devices_(devices), platform_(platform), keyboard_(platform) {}

  bool is_key_down(Key key) const override { return devices_.is_key_down(key) || holds(key); }
  MouseSample mouse() const override { return devices_.mouse(); }
  bool touch_pointer() const override { return devices_.touch_pointer(); }
  GamepadSample gamepad(std::size_t pad) const override { return devices_.gamepad(pad); }
  // What the devices typed, then the keyboard's letters this frame takes.
  std::string typed_text() const override;
  const TouchLayout* touch_layout() const override { return devices_.touch_layout(); }
  void show_touch_controls(bool show) override { devices_.show_touch_controls(show); }
  void fit_surface(float width, float height) override { devices_.fit_surface(width, height); }
  // Shows or hides the keyboard, and hands the frame starting what was typed for it.
  void request_text_input(const TextInputRequest& request) override;
  bool soft_keyboard_shown() const override { return keyboard_.shown(); }

  // A letter typed, as a Unicode code point: control characters and what is no letter at all are
  // left out.
  void type(char32_t letter);
  // An editing key going down, coming up, or both at once (a key event that only says it was hit).
  void press(Key key);
  void release(Key key) { held_.erase(key); }
  void hit(Key key);
  void back() { keyboard_.back_pressed(); }
  void leave_screen() { keyboard_.leave_screen(); }

 private:
  // A letter typed, or an editing key pressed.
  struct Typing {
    char32_t letter = 0;
    std::optional<Key> key;
  };

  // Whether the keyboard holds a key: it is down, or this frame took its press.
  bool holds(Key key) const { return held_.count(key) != 0 || this_frame_ == key; }
  bool letter_next() const { return !typed_.empty() && !typed_.front().key; }
  // Whether this frame takes an editing key: one is waiting (after the letters taken), and the
  // frame before took none (`key_free`).
  bool key_due(bool key_free) const { return key_free && !typed_.empty(); }
  void hand_out_letters();

  InputSource& devices_;
  SoftKeyboardPlatform& platform_;
  SoftKeyboard keyboard_;
  std::deque<Typing> typed_;  // what was typed that no frame took yet, in order
  // The letters this frame took, as UTF-8 (reading the text typed empties it).
  mutable std::string frame_text_;
  std::optional<Key> this_frame_;  // the editing key this frame took
  std::set<Key> held_;
};

}  // namespace redgem
