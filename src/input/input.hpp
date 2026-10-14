#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "input/devices.hpp"
#include "input/input_source.hpp"
#include "input/keys.hpp"

namespace redgem {

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
