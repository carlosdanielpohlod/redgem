#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "input/keys.hpp"

namespace redgem {

// Where raw key states come from: the real keyboard or a test script.
class InputSource {
 public:
  virtual ~InputSource() = default;
  virtual bool is_key_down(Key key) const = 0;
};

// Keys held down by a test (system tests drive the game through this).
class ScriptedInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override { return held_.count(key) != 0; }
  void set_key(Key key, bool down);
  void release_all() { held_.clear(); }

 private:
  std::set<Key> held_;
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

 private:
  InputMap map_;
  std::set<Key> current_;
  std::set<Key> previous_;
};

}  // namespace redgem
