#include "input/player_bindings.hpp"

#include <vector>

namespace redgem {
namespace {

constexpr std::size_t kFirstPlayer = 0;  // the one the keyboard drives unless told otherwise

// The sides a change is for: the one said, or both.
std::vector<AxisSide> sides(std::optional<AxisSide> side) {
  if (side) return {*side};
  return {AxisSide::Negative, AxisSide::Positive};
}

void change(ActionBinding& binding, const InputBinding& one, bool adding) {
  if (adding) return binding.add(one);
  binding.remove(one);
}

}  // namespace

PlayerBindings::PlayerBindings(InputMap declared)
    : declared_(std::move(declared)), without_keys_(declared_.without_keys()) {}

void PlayerBindings::declare_action(const std::string& name, ActionBinding binding) {
  declared_.forget(name);  // (a name is an action or an axis, never both)
  declared_.bind_action(name, std::move(binding));
  without_keys_ = declared_.without_keys();
}

void PlayerBindings::declare_axis(const std::string& name, AxisBinding binding) {
  declared_.forget(name);
  declared_.bind_axis(name, std::move(binding));
  without_keys_ = declared_.without_keys();
}

void PlayerBindings::declare(const InputMap& map) {
  for (const auto& [name, binding] : map.actions()) declare_action(name, binding);
  for (const auto& [name, binding] : map.axes()) declare_axis(name, binding);
}

const InputMap& PlayerBindings::starting(std::size_t pad) const {
  return pad == kFirstPlayer ? declared_ : without_keys_;
}

const ActionBinding* PlayerBindings::action(std::size_t pad, const std::string& name) const {
  if (!is_action(name)) return nullptr;
  const ActionBinding* changed = changes_.at(pad).action(name);
  return changed != nullptr ? changed : starting(pad).action(name);
}

const AxisBinding* PlayerBindings::axis(std::size_t pad, const std::string& name) const {
  if (!is_axis(name)) return nullptr;
  const AxisBinding* changed = changes_.at(pad).axis(name);
  return changed != nullptr ? changed : starting(pad).axis(name);
}

bool PlayerBindings::bind(std::size_t pad, const std::string& name, const InputBinding& binding,
                          std::optional<AxisSide> side) {
  if (is_axis(name)) {
    if (!side) return false;  // (which way the binding pushes the axis has to be said)
    changed_axis(pad, name, binding, *side, true);
    return true;
  }
  if (side) return false;
  return changed_action(pad, name, binding, true);
}

bool PlayerBindings::unbind(std::size_t pad, const std::string& name, const std::optional<InputBinding>& binding,
                            std::optional<AxisSide> side) {
  if (!binding) return cleared(pad, name, side);
  if (!is_axis(name)) return !side && changed_action(pad, name, *binding, false);
  for (const AxisSide which : sides(side)) changed_axis(pad, name, *binding, which, false);
  return true;
}

void PlayerBindings::reset(std::size_t pad, const std::optional<std::string>& name) {
  if (name) return changes_.at(pad).forget(*name);
  changes_.at(pad) = InputMap();
}

bool PlayerBindings::changed_action(std::size_t pad, const std::string& name, const InputBinding& binding,
                                    bool adding) {
  const ActionBinding* now = action(pad, name);
  if (now == nullptr) return false;
  ActionBinding changed = *now;
  change(changed, binding, adding);
  changes_.at(pad).bind_action(name, std::move(changed));
  return true;
}

void PlayerBindings::changed_axis(std::size_t pad, const std::string& name, const InputBinding& binding, AxisSide side,
                                  bool adding) {
  AxisBinding changed = *axis(pad, name);
  change(changed.side(side), binding, adding);
  changes_.at(pad).bind_axis(name, std::move(changed));
}

bool PlayerBindings::cleared(std::size_t pad, const std::string& name, std::optional<AxisSide> side) {
  if (is_action(name)) {
    if (side) return false;
    changes_.at(pad).bind_action(name, {});
    return true;
  }
  if (!is_axis(name)) return false;
  AxisBinding changed = *axis(pad, name);
  for (const AxisSide which : sides(side)) changed.side(which) = {};
  changes_.at(pad).bind_axis(name, std::move(changed));
  return true;
}

}  // namespace redgem
