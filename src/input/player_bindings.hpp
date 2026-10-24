#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "input/input_map.hpp"

namespace redgem {

// What each player's actions and axes are bound to. The game declares the names and what they
// start bound to (game.json, Input.define_action); each player, one per gamepad, may then bind
// them to other keys and buttons. A player's changes are kept apart from the game's bindings: they
// can be saved, undone, and loaded before the game has declared the names they are for.
//
// The first player (pad 0) starts with the game's bindings, keyboard and gamepad. The others start
// with the gamepad's alone: the keyboard drives the first player unless a player binds keys of
// their own (two players sharing a keyboard). One key may trigger several actions: nothing stops
// a game or a player from binding it twice, and every action it is bound to sees it.
class PlayerBindings {
 public:
  explicit PlayerBindings(InputMap declared = InputMap::defaults());

  // A name declared again is bound anew (for every player that did not change it).
  void declare_action(const std::string& name, ActionBinding binding);
  void declare_axis(const std::string& name, AxisBinding binding);
  void declare(const InputMap& map);
  bool is_action(const std::string& name) const { return declared_.action(name) != nullptr; }
  bool is_axis(const std::string& name) const { return declared_.axis(name) != nullptr; }
  // What the game declared, as it declared it.
  const InputMap& declared() const { return declared_; }

  // What a player's action or axis is bound to now; null for a name the game never declared.
  const ActionBinding* action(std::size_t pad, const std::string& name) const;
  const AxisBinding* axis(std::size_t pad, const std::string& name) const;

  // Adds a binding to what a player's action (no side) or a side of an axis is bound to. False,
  // changing nothing, for a name never declared or the wrong kind of name for `side`.
  bool bind(std::size_t pad, const std::string& name, const InputBinding& binding, std::optional<AxisSide> side);
  // Takes one binding away, or every key and button (an axis keeps its stick) when none is said;
  // from both sides of an axis when no side is said. False as bind is.
  bool unbind(std::size_t pad, const std::string& name, const std::optional<InputBinding>& binding,
              std::optional<AxisSide> side);
  // Back to the game's bindings: one name, or all of them.
  void reset(std::size_t pad, const std::optional<std::string>& name);

  // What a player changed (the names they bound anew, each with all it is bound to now).
  const InputMap& changes(std::size_t pad) const { return changes_.at(pad); }
  void set_changes(std::size_t pad, InputMap changes) { changes_.at(pad) = std::move(changes); }

 private:
  // What a player has before changing anything.
  const InputMap& starting(std::size_t pad) const;
  bool changed_action(std::size_t pad, const std::string& name, const InputBinding& binding, bool adding);
  void changed_axis(std::size_t pad, const std::string& name, const InputBinding& binding, AxisSide side, bool adding);
  bool cleared(std::size_t pad, const std::string& name, std::optional<AxisSide> side);

  InputMap declared_;
  InputMap without_keys_;  // declared_ for the players after the first
  std::array<InputMap, kMaxGamepads> changes_;
};

}  // namespace redgem
