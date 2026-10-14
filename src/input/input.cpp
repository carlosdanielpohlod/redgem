#include "input/input.hpp"

#include <cmath>

#include <algorithm>

namespace redgem {
namespace {

constexpr float kStickDeadZone = 0.2f;  // of a stick's travel
constexpr float kAxisNegative = -1.0f;
constexpr float kAxisPositive = 1.0f;

bool any_down(const InputState& input, const std::vector<Key>& keys) {
  return std::any_of(keys.begin(), keys.end(), [&input](Key key) { return input.down(key); });
}

// Where the keys held put an axis: -1, 0 (none, or both ways at once) or 1.
float key_push(const InputState& input, const AxisBinding& binding) {
  float value = 0.0f;
  if (any_down(input, binding.negative)) value += kAxisNegative;
  if (any_down(input, binding.positive)) value += kAxisPositive;
  return value;
}

// Where the binding's stick puts an axis (and by how much: it is not just on or off).
float stick_push(const InputState& input, const AxisBinding& binding) {
  if (!binding.stick) return 0.0f;
  if (!input.gamepad_connected()) return 0.0f;
  const float pushed = input.gamepad_axis(*binding.stick);
  if (pushed == 0.0f) return 0.0f;
  return pushed * (binding.stick_inverted ? kAxisNegative : kAxisPositive);
}

bool is_trigger(GamepadAxis axis) { return axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger; }

// Whether any of an action's keys or gamepad buttons is held.
bool held(const InputState& input, const ActionBinding& binding) {
  return any_down(input, binding.keys) ||
         std::any_of(binding.buttons.begin(), binding.buttons.end(),
                     [&input](GamepadButton button) { return input.gamepad_down(button); });
}

// Whether any of them went down this frame.
bool just_pressed(const InputState& input, const ActionBinding& binding) {
  return std::any_of(binding.keys.begin(), binding.keys.end(), [&input](Key key) { return input.pressed(key); }) ||
         std::any_of(binding.buttons.begin(), binding.buttons.end(),
                     [&input](GamepadButton button) { return input.gamepad_pressed(button); });
}

}  // namespace

InputMap InputMap::defaults() {
  InputMap map;
  map.bind_axis("horizontal", {{Key::A, Key::Left}, {Key::D, Key::Right}, GamepadAxis::LeftX, false});
  map.bind_axis("vertical", {{Key::S, Key::Down}, {Key::W, Key::Up}, GamepadAxis::LeftY, true});
  map.bind_action("jump", {{Key::Space}, {GamepadButton::A}});
  map.bind_action("fire", {{Key::LeftControl, Key::Enter}, {GamepadButton::X}});
  map.bind_action("run", {{Key::LeftShift}, {GamepadButton::B}});
  return map;
}

const AxisBinding* InputMap::axis(const std::string& name) const {
  const auto it = axes_.find(name);
  return it == axes_.end() ? nullptr : &it->second;
}

const ActionBinding* InputMap::action(const std::string& name) const {
  const auto it = actions_.find(name);
  return it == actions_.end() ? nullptr : &it->second;
}

void InputState::update(const InputSource& source) {
  previous_ = std::move(current_);
  current_.clear();
  for (const Key key : all_keys()) {
    if (source.is_key_down(key)) current_.insert(key);
  }
  previous_mouse_ = mouse_;
  mouse_ = source.mouse();
  previous_gamepad_ = gamepad_;
  gamepad_ = source.gamepad();
  typed_ = source.typed_text();
}

float InputState::gamepad_axis(GamepadAxis axis) const {
  const float value = gamepad_.axes[static_cast<std::size_t>(axis)];
  // (Worn sticks drift around their middle; triggers rest at their end.)
  const bool drifting = !is_trigger(axis) && std::fabs(value) < kStickDeadZone;
  return drifting ? 0.0f : value;
}

std::optional<float> InputState::axis(const std::string& name) const {
  const AxisBinding* binding = map_.axis(name);
  if (binding == nullptr) return std::nullopt;
  // The keyboard wins; with no key held, the stick decides.
  const float keys = key_push(*this, *binding);
  return keys != 0.0f ? keys : stick_push(*this, *binding);
}

std::optional<bool> InputState::action_down(const std::string& name) const {
  const ActionBinding* binding = map_.action(name);
  if (binding == nullptr) return std::nullopt;
  return held(*this, *binding);
}

std::optional<bool> InputState::action_pressed(const std::string& name) const {
  const ActionBinding* binding = map_.action(name);
  if (binding == nullptr) return std::nullopt;
  return just_pressed(*this, *binding);
}

}  // namespace redgem
