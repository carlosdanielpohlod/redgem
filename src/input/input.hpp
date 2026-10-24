#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "input/devices.hpp"
#include "input/input_map.hpp"
#include "input/input_source.hpp"
#include "input/keys.hpp"
#include "input/player_bindings.hpp"

namespace redgem {

// A key or gamepad button that went down while a capture waited for one (Input.capture_next), and
// the gamepad it is on (none for a key).
struct CapturedBinding {
  InputBinding binding;
  std::optional<std::size_t> pad;
};

// The first gamepad: the one read when no other is asked for, and the first player's.
inline constexpr std::size_t kFirstGamepad = 0;

// Per-frame snapshot: down / pressed this frame / released this frame.
class InputState {
 public:
  explicit InputState(InputMap map = InputMap::defaults()) : bindings_(std::move(map)) {}

  void update(const InputSource& source);

  bool down(Key key) const { return current_.count(key) != 0; }
  bool pressed(Key key) const { return down(key) && previous_.count(key) == 0; }
  bool released(Key key) const { return !down(key) && previous_.count(key) != 0; }
  // -1..1, or nullopt if the axis is unknown. Each takes the player whose bindings are read, by
  // their gamepad's number.
  std::optional<float> axis(const std::string& name, std::size_t pad = kFirstGamepad) const;
  std::optional<bool> action_down(const std::string& name, std::size_t pad = kFirstGamepad) const;
  // Whether any of the action's keys or buttons went down this frame.
  std::optional<bool> action_pressed(const std::string& name, std::size_t pad = kFirstGamepad) const;
  // Whether the action stopped being held this frame.
  std::optional<bool> action_released(const std::string& name, std::size_t pad = kFirstGamepad) const;

  // What the actions and axes are bound to, for every player.
  PlayerBindings& bindings() { return bindings_; }
  const PlayerBindings& bindings() const { return bindings_; }

  // Waits for the next key or gamepad button to go down (one held already does not count): the
  // first update that sees one keeps it for take_captured, and stops waiting.
  void capture_next() { capturing_ = true, captured_.reset(); }
  void cancel_capture() { capturing_ = false; }
  bool capturing() const { return capturing_; }
  // What a capture caught, once: nothing again until the next one catches something.
  std::optional<CapturedBinding> take_captured();

  const std::string& typed_text() const { return typed_; }

  bool gamepad_connected(std::size_t pad = kFirstGamepad) const { return gamepads_.at(pad).connected; }
  // The numbers of the gamepads plugged in, smallest first.
  std::vector<std::size_t> connected_pads() const;
  // A stick or trigger; small stick movements around the center read as 0 (worn sticks drift).
  float gamepad_axis(GamepadAxis axis, std::size_t pad = kFirstGamepad) const;
  bool gamepad_down(GamepadButton button, std::size_t pad = kFirstGamepad) const {
    return gamepads_.at(pad).buttons[static_cast<std::size_t>(button)];
  }
  bool gamepad_pressed(GamepadButton button, std::size_t pad = kFirstGamepad) const {
    return gamepad_down(button, pad) && !previous_gamepads_.at(pad).buttons[static_cast<std::size_t>(button)];
  }
  bool gamepad_released(GamepadButton button, std::size_t pad = kFirstGamepad) const {
    return !gamepad_down(button, pad) && previous_gamepads_.at(pad).buttons[static_cast<std::size_t>(button)];
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
  using Gamepads = std::array<GamepadSample, kMaxGamepads>;

  // Whether an action is held with these keys down and this gamepad as it is.
  static bool held(const std::set<Key>& keys, const GamepadSample& pad, const ActionBinding& binding);
  bool just_pressed(const ActionBinding& binding, std::size_t pad) const;
  float key_push(const AxisBinding& binding, std::size_t pad) const;
  float stick_push(const AxisBinding& binding, std::size_t pad) const;
  // A capture waiting for a key or button catches the first that went down this frame.
  void catch_capture();
  std::optional<CapturedBinding> first_pressed() const;
  std::optional<CapturedBinding> first_pressed_button(std::size_t pad) const;

  PlayerBindings bindings_;
  std::set<Key> current_;
  std::set<Key> previous_;
  MouseSample mouse_;
  MouseSample previous_mouse_;
  Gamepads gamepads_{};
  Gamepads previous_gamepads_{};
  std::string typed_;
  bool capturing_ = false;
  std::optional<CapturedBinding> captured_;
};

}  // namespace redgem
