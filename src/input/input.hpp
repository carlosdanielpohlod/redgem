#pragma once

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "input/keys.hpp"

namespace redgem {

enum class MouseButton { Left, Right, Middle };
inline constexpr std::size_t kMouseButtonCount = 3;

// "left" -> MouseButton::Left; nullopt for an unknown name.
std::optional<MouseButton> mouse_button_from_name(const std::string& name);

// The mouse at one instant: where it is (pixels from the top-left of what the game is drawn in),
// how far the wheel turned since the last frame, which buttons are held.
struct MouseSample {
  float x = 0.0f;
  float y = 0.0f;
  float wheel = 0.0f;
  std::array<bool, kMouseButtonCount> buttons{};
};

// A gamepad's sticks and triggers, and its buttons (named as on an Xbox pad: A is the bottom one).
enum class GamepadAxis { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger };
enum class GamepadButton {
  A, B, X, Y, LeftBumper, RightBumper, Back, Start, DpadUp, DpadDown, DpadLeft, DpadRight, LeftStick, RightStick
};
inline constexpr std::size_t kGamepadAxisCount = 6;
inline constexpr std::size_t kGamepadButtonCount = 14;

// "left_x" -> GamepadAxis::LeftX, "a" -> GamepadButton::A; nullopt for an unknown name.
std::optional<GamepadAxis> gamepad_axis_from_name(const std::string& name);
std::optional<GamepadButton> gamepad_button_from_name(const std::string& name);

// The first gamepad at one instant. Sticks go from -1 to 1 (left and up are negative), triggers
// from 0 to 1.
struct GamepadSample {
  bool connected = false;
  std::array<float, kGamepadAxisCount> axes{};
  std::array<bool, kGamepadButtonCount> buttons{};
};

// Where raw key, mouse and gamepad states come from: the real devices or a test script.
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
};

// Keys held down by a test (system tests drive the game through this).
class ScriptedInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override { return held_.count(key) != 0; }
  MouseSample mouse() const override { return mouse_; }
  GamepadSample gamepad() const override { return gamepad_; }
  std::string typed_text() const override { return typed_; }
  // What the test "types" every frame until told otherwise.
  void type_text(std::string text) { typed_ = std::move(text); }
  // Either of these plugs the test's gamepad in.
  void set_gamepad_axis(GamepadAxis axis, float value) {
    gamepad_.connected = true;
    gamepad_.axes[static_cast<std::size_t>(axis)] = value;
  }
  void set_gamepad_button(GamepadButton button, bool down) {
    gamepad_.connected = true;
    gamepad_.buttons[static_cast<std::size_t>(button)] = down;
  }
  void set_key(Key key, bool down);
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

struct AxisBinding {
  std::vector<Key> negative;
  std::vector<Key> positive;
  // A gamepad stick that moves the axis too, and whether it runs the other way (a stick pushed up
  // reads negative, while "vertical" is positive forward).
  std::optional<GamepadAxis> stick;
  bool stick_inverted = false;
};

// What triggers an action: any of these keys or gamepad buttons.
struct ActionBinding {
  std::vector<Key> keys;
  std::vector<GamepadButton> buttons;
};

// Named axes and actions on top of raw keys.
class InputMap {
 public:
  static InputMap defaults();

  void bind_axis(const std::string& name, AxisBinding binding) { axes_[name] = std::move(binding); }
  void bind_action(const std::string& name, ActionBinding binding) { actions_[name] = std::move(binding); }
  const AxisBinding* axis(const std::string& name) const;
  const ActionBinding* action(const std::string& name) const;

 private:
  std::map<std::string, AxisBinding> axes_;
  std::map<std::string, ActionBinding> actions_;
};

// Per-frame snapshot: down / pressed this frame / released this frame.
class InputState {
 public:
  explicit InputState(InputMap map = InputMap::defaults()) : map_(std::move(map)) {}

  void update(const InputSource& source);

  bool down(Key key) const { return current_.count(key) != 0; }
  bool pressed(Key key) const { return down(key) && previous_.count(key) == 0; }
  bool released(Key key) const { return !down(key) && previous_.count(key) != 0; }
  // -1..1, or nullopt if the axis is unknown.
  std::optional<float> axis(const std::string& name) const;
  std::optional<bool> action_down(const std::string& name) const;
  std::optional<bool> action_pressed(const std::string& name) const;

  const std::string& typed_text() const { return typed_; }

  bool gamepad_connected() const { return gamepad_.connected; }
  // A stick or trigger; small stick movements around the center read as 0 (worn sticks drift).
  float gamepad_axis(GamepadAxis axis) const;
  bool gamepad_down(GamepadButton button) const { return gamepad_.buttons[static_cast<std::size_t>(button)]; }
  bool gamepad_pressed(GamepadButton button) const {
    return gamepad_down(button) && !previous_gamepad_.buttons[static_cast<std::size_t>(button)];
  }
  bool gamepad_released(GamepadButton button) const {
    return !gamepad_down(button) && previous_gamepad_.buttons[static_cast<std::size_t>(button)];
  }

  float mouse_x() const { return mouse_.x; }
  float mouse_y() const { return mouse_.y; }
  // How far the mouse moved since the previous frame.
  float mouse_dx() const { return mouse_.x - previous_mouse_.x; }
  float mouse_dy() const { return mouse_.y - previous_mouse_.y; }
  float mouse_wheel() const { return mouse_.wheel; }
  bool mouse_down(MouseButton button) const { return mouse_.buttons[static_cast<std::size_t>(button)]; }
  bool mouse_pressed(MouseButton button) const {
    return mouse_down(button) && !previous_mouse_.buttons[static_cast<std::size_t>(button)];
  }
  bool mouse_released(MouseButton button) const {
    return !mouse_down(button) && previous_mouse_.buttons[static_cast<std::size_t>(button)];
  }

 private:
  InputMap map_;
  std::set<Key> current_;
  std::set<Key> previous_;
  MouseSample mouse_;
  MouseSample previous_mouse_;
  GamepadSample gamepad_;
  GamepadSample previous_gamepad_;
  std::string typed_;
};

}  // namespace redgem
