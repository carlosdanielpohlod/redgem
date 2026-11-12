#pragma once

#include <android/input.h>
#include <android_native_app_glue.h>

#include <cstdint>

#include "input/soft_keyboard.hpp"
#include "input/soft_keyboard_input_source.hpp"

namespace redgem {

// The system's on-screen keyboard over the activity: shown and hidden through Java's
// InputMethodManager on the window's decor view (ANativeActivity's own calls when that does not
// take). And the activity's key events, read before raylib reads them: the letters they type
// (asked of Java's KeyEvent, raylib turning none into letters), the editing keys and the back key
// go to the SoftKeyboardInputSource it feeds. Everything else reaches raylib as before. There is
// one per game: it is the activity's.
class AndroidSoftKeyboard final : public SoftKeyboardPlatform {
 public:
  // `app` has to outlive this.
  explicit AndroidSoftKeyboard(android_app* app) : app_(app) {}
  // Hands raylib its key events back.
  ~AndroidSoftKeyboard() override;
  AndroidSoftKeyboard(const AndroidSoftKeyboard&) = delete;
  AndroidSoftKeyboard& operator=(const AndroidSoftKeyboard&) = delete;

  // Where the key events go; it has to outlive this.
  void feed(SoftKeyboardInputSource& source) { source_ = &source; }
  void show() override;
  void hide() override;
  // Puts this in front of raylib's reading of the input events, once raylib has set it up (its
  // window opens after the game starts).
  void begin_frame() override;

  // Reads an input event before raylib does: true when it was the keyboard's, and raylib must not
  // see it.
  bool takes(const AInputEvent* event);
  // Has raylib read an input event.
  int32_t hand_to_raylib(android_app* app, AInputEvent* event) const { return raylib_reader_(app, event); }

 private:
  // A key event that is no back key: its letters typed, its editing key pressed or let go.
  void hand_on(const AInputEvent* event);
  // An editing key going down, coming up, or hit (an AKEY_EVENT_ACTION_*).
  void hand_on_editing(Key key, int32_t action);
  void type_letters(const AInputEvent* event);
  // Whether this reads the activity's input events now.
  bool reading() const;
  // Whether this is to start reading them: it does not yet, it has somewhere to send what it reads,
  // and raylib has set up its own reading to hand the rest to.
  bool ready_to_read() const;

  android_app* app_;
  SoftKeyboardInputSource* source_ = nullptr;
  int32_t (*raylib_reader_)(android_app*, AInputEvent*) = nullptr;
};

}  // namespace redgem
