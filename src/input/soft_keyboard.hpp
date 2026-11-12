#pragma once

#include "input/input_source.hpp"

namespace redgem {

// What shows and hides a touch screen's on-screen keyboard: the system's, on a phone
// (app/android/android_soft_keyboard.hpp); one that only counts, in tests.
class SoftKeyboardPlatform {
 public:
  virtual ~SoftKeyboardPlatform() = default;
  virtual void show() = 0;
  virtual void hide() = 0;
  // Called once a frame, before that frame's input is read.
  virtual void begin_frame() {}
};

// When an on-screen keyboard is up: while the game draws a text field, unless the player put it
// away with the back key and has not tapped a field since. It goes once a frame draws no field
// (the field closed, the menu holding it closed, the scene changed).
class SoftKeyboard {
 public:
  // `platform` has to outlive this.
  explicit SoftKeyboard(SoftKeyboardPlatform& platform) : platform_(platform) {}

  // Shows or hides the keyboard for what the last frame's text fields asked.
  void follow(const TextInputRequest& request);
  // A back key going down (`down`) or up: true when the press is the keyboard's (it was up, and it
  // goes), so that the game does not see it. Its release, and the repeats of it held, are too.
  bool take_back(bool down);
  bool shown() const { return shown_; }

 private:
  // Whether the keyboard is to be up for what the fields asked.
  bool wanted(const TextInputRequest& request) const { return request.field_shown && !put_away_; }
  // Whether the fields asked for a keyboard the player put away to come back: a field was tapped,
  // or none is drawn any longer (the next one starts afresh).
  static bool brings_back(const TextInputRequest& request) { return request.field_tapped || !request.field_shown; }
  // Whether a wanted keyboard is to be shown now: it is not up yet, or a field was tapped.
  bool asks_to_show(const TextInputRequest& request) const { return request.field_tapped || !shown_; }
  // Whether a back key going `down` (or up) closes the keyboard: it goes down while it is up.
  bool closed_by(bool down) const { return down && shown_; }
  void show();
  void hide();

  SoftKeyboardPlatform& platform_;
  bool shown_ = false;
  bool put_away_ = false;   // the player closed it with the back key
  bool back_held_ = false;  // the back key that closed it is still down
};

}  // namespace redgem
