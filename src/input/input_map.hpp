#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "input/devices.hpp"
#include "input/keys.hpp"

// What triggers a game's actions and moves its axes: the names a game gives them, and the keys
// and gamepad buttons behind each name.
namespace redgem {

// One key or one gamepad button. As text (game.json, the player's saved bindings, Ruby):
// "key:space", "pad:a".
using InputBinding = std::variant<Key, GamepadButton>;

std::optional<InputBinding> binding_from_text(const std::string& text);
std::string binding_text(const InputBinding& binding);

// What triggers an action: any of these keys or gamepad buttons.
struct ActionBinding {
  std::vector<Key> keys;
  std::vector<GamepadButton> buttons;

  // Adds what is not there yet (a binding is never listed twice).
  void add(const InputBinding& binding);
  void remove(const InputBinding& binding);
  bool empty() const { return keys.empty() && buttons.empty(); }
  // Keys first, then buttons, each in the order they were added.
  std::vector<InputBinding> all() const;
};

// The two ways an axis goes.
enum class AxisSide { Negative, Positive };

struct AxisBinding {
  ActionBinding negative;
  ActionBinding positive;
  // A gamepad stick that moves the axis too, and whether it runs the other way (a stick pushed up
  // reads negative, while "vertical" is positive forward).
  std::optional<GamepadAxis> stick;
  bool stick_inverted = false;

  ActionBinding& side(AxisSide which) { return which == AxisSide::Negative ? negative : positive; }
};

// Named axes and actions on top of raw keys.
class InputMap {
 public:
  // What every game has without declaring anything: the axes "horizontal" and "vertical" and the
  // actions "jump", "fire" and "run".
  static InputMap defaults();

  void bind_axis(const std::string& name, AxisBinding binding) { axes_[name] = std::move(binding); }
  void bind_action(const std::string& name, ActionBinding binding) { actions_[name] = std::move(binding); }
  const AxisBinding* axis(const std::string& name) const;
  const ActionBinding* action(const std::string& name) const;
  // Forgets what a name was bound to here (the name stays known wherever else it is): as either
  // kind, or as one of them.
  void forget(const std::string& name);
  void forget_axis(const std::string& name) { axes_.erase(name); }
  void forget_action(const std::string& name) { actions_.erase(name); }
  bool empty() const { return axes_.empty() && actions_.empty(); }

  const std::map<std::string, AxisBinding>& axes() const { return axes_; }
  const std::map<std::string, ActionBinding>& actions() const { return actions_; }

  // The same names with the gamepad's buttons and sticks only: what a second player starts with,
  // the keyboard being the first player's.
  InputMap without_keys() const;

 private:
  std::map<std::string, AxisBinding> axes_;
  std::map<std::string, ActionBinding> actions_;
};

}  // namespace redgem
