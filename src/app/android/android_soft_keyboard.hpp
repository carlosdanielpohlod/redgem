#pragma once

#include <android/input.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <cstdint>

#include "input/soft_keyboard.hpp"
#include "input/soft_keyboard_input_source.hpp"

namespace redgem {

// The game's thread attached to Java's virtual machine for as long as this lives (and let go
// after, when it was not attached before). No environment when it cannot attach.
class JavaThread {
 public:
  explicit JavaThread(JavaVM* vm);
  ~JavaThread();
  JavaThread(const JavaThread&) = delete;
  JavaThread& operator=(const JavaThread&) = delete;
  JNIEnv* env() const { return env_; }

 private:
  JavaVM* vm_;
  JNIEnv* env_ = nullptr;
  bool attached_here_ = false;
};

// The system's on-screen keyboard over the activity: shown and hidden through Java's
// InputMethodManager on the window's decor view (ANativeActivity's own calls when that is
// refused). And the activity's key events and commands, read before raylib reads them: the
// letters keys type (asked of Java's KeyEvent, raylib turning none into letters), the editing
// keys, the back key and the app leaving the screen go to the SoftKeyboardInputSource it feeds.
// raylib still gets every one of them. There is one per game: it is the activity's. It lives on
// the game's thread, which raylib reads the events on.
class AndroidSoftKeyboard final : public SoftKeyboardPlatform {
 public:
  // `app` has to outlive this.
  explicit AndroidSoftKeyboard(android_app* app) : app_(app), java_(app->activity->vm) {}
  // Hands raylib its events back.
  ~AndroidSoftKeyboard() override;
  AndroidSoftKeyboard(const AndroidSoftKeyboard&) = delete;
  AndroidSoftKeyboard& operator=(const AndroidSoftKeyboard&) = delete;

  // Where the key events go; it has to outlive this.
  void feed(SoftKeyboardInputSource& source) { source_ = &source; }
  void show() override;
  void hide() override;
  // Puts this in front of raylib's reading of the events, once raylib has set it up (its window
  // opens after the game starts).
  void begin_frame() override;

  // Reads an input event, then has raylib read it.
  int32_t read_input(android_app* app, AInputEvent* event);
  // Reads a command of the activity, then has raylib read it.
  void read_command(android_app* app, int32_t command);

 private:
  // A key event of a keyboard: its letters typed, its editing key or back key handed on.
  void hand_on(const AInputEvent* event);
  // An editing key going down, coming up, or hit (an ACTION_MULTIPLE, as many times as it says).
  void hand_on_editing(Key key, const AInputEvent* event);
  void type_letters(const AInputEvent* event);
  // The letter a key types with the modifier keys held, on the keyboard the event came from.
  char32_t letter_of(const AInputEvent* event) const;
  // Whether this reads the activity's events now.
  bool reading() const;
  // Whether raylib has set up its own reading of the events, to hand them on to.
  bool raylib_reads() const;
  // Whether this is to start reading them: raylib does, this does not yet, and it has somewhere to
  // send what it reads.
  bool ready_to_read() const;

  android_app* app_;
  JavaThread java_;
  SoftKeyboardInputSource* source_ = nullptr;
  int32_t (*raylib_input_)(android_app*, AInputEvent*) = nullptr;
  void (*raylib_command_)(android_app*, int32_t) = nullptr;
};

}  // namespace redgem
