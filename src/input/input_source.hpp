#pragma once

#include <set>
#include <string>
#include <utility>

#include "input/devices.hpp"
#include "input/keys.hpp"

namespace redgem {

// Where raw key, mouse and gamepad states come from: the real devices, or code that says what is
// pressed.
class InputSource {
 public:
  virtual ~InputSource() = default;
  virtual bool is_key_down(Key key) const = 0;
  virtual MouseSample mouse() const { return {}; }
  virtual GamepadSample gamepad() const { return {}; }
  // What was typed since the last frame, as text (UTF-8): letters as the keyboard layout gives them.
  virtual std::string typed_text() const { return {}; }
  // The on-screen controls to draw, on a touch screen showing them; null otherwise.
  virtual const struct TouchLayout* touch_layout() const { return nullptr; }
  // Shows or hides a touch screen's controls (a game with a touch interface of its own hides them).
  virtual void show_touch_controls(bool) {}
  // Says how large what the game is drawn on is, in pixels, every frame: what is laid out over
  // the game (a touch screen's controls) is laid out on that, wherever on a window it sits.
  virtual void fit_surface(float, float) {}
};

// Input fed by code: keys, a mouse and a gamepad that are wherever the program driving the game
// last put them (the editor's play mode, anything that plays a game without a person).
class ScriptedInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override { return held_.count(key) != 0; }
  MouseSample mouse() const override { return mouse_; }
  GamepadSample gamepad() const override { return gamepad_; }
  std::string typed_text() const override { return typed_; }
  // What is "typed" every frame until told otherwise.
  void type_text(std::string text) { typed_ = std::move(text); }
  // Either of these plugs the gamepad in.
  void set_gamepad_axis(GamepadAxis axis, float value) {
    gamepad_.connected = true;
    gamepad_.axes[static_cast<std::size_t>(axis)] = value;
  }
  void set_gamepad_button(GamepadButton button, bool down) {
    gamepad_.connected = true;
    gamepad_.buttons[static_cast<std::size_t>(button)] = down;
  }
  void set_key(Key key, bool down) {
    held_.erase(key);
    if (down) held_.insert(key);
  }
  void move_mouse(float x, float y) { mouse_.x = x, mouse_.y = y; }
  void set_mouse_button(MouseButton button, bool down) { mouse_.buttons[static_cast<std::size_t>(button)] = down; }
  void set_mouse_wheel(float notches) { mouse_.wheel = notches; }
  void release_all() {
    held_.clear();
    mouse_.buttons = {};
    mouse_.wheel = 0.0f;
    gamepad_ = {};
    typed_.clear();
  }

 private:
  std::set<Key> held_;
  MouseSample mouse_;
  GamepadSample gamepad_;
  std::string typed_;
};

}  // namespace redgem
