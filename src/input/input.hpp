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

// Where raw key and mouse states come from: the real devices or a test script.
class InputSource {
 public:
  virtual ~InputSource() = default;
  virtual bool is_key_down(Key key) const = 0;
  virtual MouseSample mouse() const { return {}; }
};

// Keys held down by a test (system tests drive the game through this).
class ScriptedInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override { return held_.count(key) != 0; }
  MouseSample mouse() const override { return mouse_; }
  void set_key(Key key, bool down);
  void move_mouse(float x, float y) { mouse_.x = x, mouse_.y = y; }
  void set_mouse_button(MouseButton button, bool down) { mouse_.buttons[static_cast<std::size_t>(button)] = down; }
  void set_mouse_wheel(float notches) { mouse_.wheel = notches; }
  void release_all() {
    held_.clear();
    mouse_.buttons = {};
    mouse_.wheel = 0.0f;
  }

 private:
  std::set<Key> held_;
  MouseSample mouse_;
};

struct AxisBinding {
  std::vector<Key> negative;
  std::vector<Key> positive;
};

// Named axes and actions on top of raw keys.
class InputMap {
 public:
  static InputMap defaults();

  void bind_axis(const std::string& name, AxisBinding binding) { axes_[name] = std::move(binding); }
  void bind_action(const std::string& name, std::vector<Key> keys) { actions_[name] = std::move(keys); }
  const AxisBinding* axis(const std::string& name) const;
  const std::vector<Key>* action(const std::string& name) const;

 private:
  std::map<std::string, AxisBinding> axes_;
  std::map<std::string, std::vector<Key>> actions_;
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
};

}  // namespace redgem
