#pragma once

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <utility>

#include "input/devices.hpp"
#include "input/keys.hpp"

namespace redgem {

// What the game's text fields asked of a touch screen's on-screen keyboard over a frame: whether
// one was drawn, and whether one was tapped (which brings back a keyboard the player put away).
struct TextInputRequest {
  bool field_shown = false;
  bool field_tapped = false;
};

// Where raw key, mouse and gamepad states come from: the real devices, or code that says what is
// pressed.
class InputSource {
 public:
  virtual ~InputSource() = default;
  virtual bool is_key_down(Key key) const = 0;
  virtual MouseSample mouse() const { return {}; }
  // Whether the mouse is a finger on a touch screen (a tap is its left button): a capture then
  // leaves it out, so that a tap still works the screen it is on.
  virtual bool touch_pointer() const { return false; }
  // A gamepad by its number, from 0 to kMaxGamepads - 1: unplugged when there is none.
  virtual GamepadSample gamepad(std::size_t) const { return {}; }
  // What was typed since the last frame, as text (UTF-8): letters as the keyboard layout gives them.
  virtual std::string typed_text() const { return {}; }
  // The on-screen controls to draw, on a touch screen showing them; null otherwise.
  virtual const struct TouchLayout* touch_layout() const { return nullptr; }
  // Shows or hides a touch screen's controls (a game with a touch interface of its own hides them).
  virtual void show_touch_controls(bool) {}
  // Says how large what the game is drawn on is, in pixels, every frame: what is laid out over
  // the game (a touch screen's controls) is laid out on that, wherever on a window it sits.
  virtual void fit_surface(float, float) {}
  // Says, as a frame starts, what the text fields of the frame before asked for: a touch screen
  // shows its on-screen keyboard while one is drawn. Nothing else has one to show.
  virtual void request_text_input(const TextInputRequest&) {}
  // Whether an on-screen keyboard was asked to cover the bottom of the screen.
  virtual bool soft_keyboard_shown() const { return false; }
};

// Input fed by code: keys, a mouse and a gamepad that are wherever the program driving the game
// last put them (the editor's play mode, anything that plays a game without a person).
class ScriptedInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override { return held_.count(key) != 0; }
  MouseSample mouse() const override { return mouse_; }
  bool touch_pointer() const override { return touch_pointer_; }
  // Plays a touch screen, whose taps are the mouse's left button.
  void set_touch_pointer(bool touch) { touch_pointer_ = touch; }
  GamepadSample gamepad(std::size_t pad) const override { return gamepads_.at(pad); }
  std::string typed_text() const override { return typed_; }
  // What is "typed" every frame until told otherwise.
  void type_text(std::string text) { typed_ = std::move(text); }
  void request_text_input(const TextInputRequest& request) override { text_input_ = request; }
  bool soft_keyboard_shown() const override { return keyboard_shown_; }
  // What the engine last said the text fields asked for.
  const TextInputRequest& text_input_requested() const { return text_input_; }
  // Plays a touch screen whose on-screen keyboard is up (or not).
  void set_soft_keyboard_shown(bool shown) { keyboard_shown_ = shown; }
  // Either of these plugs the gamepad in.
  void set_gamepad_axis(GamepadAxis axis, float value, std::size_t pad = 0) {
    gamepads_.at(pad).connected = true;
    gamepads_.at(pad).axes[static_cast<std::size_t>(axis)] = value;
  }
  void set_gamepad_button(GamepadButton button, bool down, std::size_t pad = 0) {
    gamepads_.at(pad).connected = true;
    gamepads_.at(pad).buttons[static_cast<std::size_t>(button)] = down;
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
    gamepads_ = {};
    typed_.clear();
  }

 private:
  std::set<Key> held_;
  MouseSample mouse_;
  bool touch_pointer_ = false;
  std::array<GamepadSample, kMaxGamepads> gamepads_{};
  std::string typed_;
  TextInputRequest text_input_;
  bool keyboard_shown_ = false;
};

}  // namespace redgem
