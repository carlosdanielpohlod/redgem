#include "input/input.hpp"

#include <cmath>

#include <algorithm>

namespace redgem {
namespace {

constexpr float kStickDeadZone = 0.2f;  // of a stick's travel
constexpr float kAxisNegative = -1.0f;
constexpr float kAxisPositive = 1.0f;

bool is_trigger(GamepadAxis axis) { return axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger; }

// Where the keys held put an axis: -1, 0 (none, or both ways at once) or 1.
float push_of(bool negative, bool positive) {
  float value = 0.0f;
  if (negative) value += kAxisNegative;
  if (positive) value += kAxisPositive;
  return value;
}

}  // namespace

std::vector<InputBinding> default_capture_cancels() { return {Key::Escape, GamepadButton::B, GamepadButton::Back}; }

bool InputState::held(const std::set<Key>& keys, const GamepadSample& pad, const ActionBinding& binding) {
  const bool key_held =
      std::any_of(binding.keys.begin(), binding.keys.end(), [&keys](Key key) { return keys.count(key) != 0; });
  return key_held || std::any_of(binding.buttons.begin(), binding.buttons.end(), [&pad](GamepadButton button) {
           return pad.buttons[static_cast<std::size_t>(button)];
         });
}

bool InputState::just_pressed(const ActionBinding& binding, std::size_t pad) const {
  const bool key_pressed =
      std::any_of(binding.keys.begin(), binding.keys.end(), [this](Key key) { return pressed(key); });
  return key_pressed || std::any_of(binding.buttons.begin(), binding.buttons.end(),
                                    [this, pad](GamepadButton button) { return gamepad_pressed(button, pad); });
}

float InputState::key_push(const AxisBinding& binding, std::size_t pad) const {
  const GamepadSample& gamepad = gamepads_.at(pad);
  return push_of(held(current_, gamepad, binding.negative), held(current_, gamepad, binding.positive));
}

// Where the binding's stick puts an axis (and by how much: it is not just on or off).
float InputState::stick_push(const AxisBinding& binding, std::size_t pad) const {
  if (!binding.stick) return 0.0f;
  if (!gamepad_connected(pad)) return 0.0f;
  const float pushed = gamepad_axis(*binding.stick, pad);
  if (pushed == 0.0f) return 0.0f;
  return pushed * (binding.stick_inverted ? kAxisNegative : kAxisPositive);
}

void InputState::update(const InputSource& source) {
  previous_ = std::move(current_);
  current_.clear();
  for (const Key key : all_keys()) {
    if (source.is_key_down(key)) current_.insert(key);
  }
  previous_mouse_ = mouse_;
  mouse_ = source.mouse();
  previous_gamepads_ = gamepads_;
  for (std::size_t pad = 0; pad < kMaxGamepads; ++pad) gamepads_[pad] = source.gamepad(pad);
  typed_ = source.typed_text();
  catch_capture();
}

void InputState::catch_capture() {
  if (!capturing_) return;
  captured_ = first_pressed();
  if (!captured_) return;
  capturing_ = false;
  captured_->cancelled = cancels_capture(captured_->binding);
  swallow_this_frame();
}

bool InputState::cancels_capture(const InputBinding& binding) const {
  return std::find(capture_cancels_.begin(), capture_cancels_.end(), binding) != capture_cancels_.end();
}

void InputState::swallow_this_frame() {
  previous_.insert(current_.begin(), current_.end());
  for (std::size_t pad = 0; pad < kMaxGamepads; ++pad) {
    for (std::size_t button = 0; button < kGamepadButtonCount; ++button) {
      previous_gamepads_[pad].buttons[button] |= gamepads_[pad].buttons[button];
    }
  }
}

std::optional<CapturedBinding> InputState::first_pressed() const {
  for (const Key key : all_keys()) {
    if (pressed(key)) return CapturedBinding{key, std::nullopt};
  }
  for (std::size_t pad = 0; pad < kMaxGamepads; ++pad) {
    if (auto caught = first_pressed_button(pad)) return caught;
  }
  return std::nullopt;
}

std::optional<CapturedBinding> InputState::first_pressed_button(std::size_t pad) const {
  for (std::size_t i = 0; i < kGamepadButtonCount; ++i) {
    const auto button = static_cast<GamepadButton>(i);
    if (gamepad_pressed(button, pad)) return CapturedBinding{button, pad};
  }
  return std::nullopt;
}

std::optional<CapturedBinding> InputState::take_captured() {
  const std::optional<CapturedBinding> caught = captured_;
  captured_.reset();
  return caught;
}

std::vector<std::size_t> InputState::connected_pads() const {
  std::vector<std::size_t> pads;
  for (std::size_t pad = 0; pad < kMaxGamepads; ++pad) {
    if (gamepad_connected(pad)) pads.push_back(pad);
  }
  return pads;
}

float InputState::gamepad_axis(GamepadAxis axis, std::size_t pad) const {
  const float value = gamepads_.at(pad).axes[static_cast<std::size_t>(axis)];
  // (Worn sticks drift around their middle; triggers rest at their end.)
  const bool drifting = !is_trigger(axis) && std::fabs(value) < kStickDeadZone;
  return drifting ? 0.0f : value;
}

std::optional<float> InputState::axis(const std::string& name, std::size_t pad) const {
  const std::optional<AxisBinding> binding = bindings_.axis(pad, name);
  if (!binding) return std::nullopt;
  // The keyboard wins; with no key held, the stick decides.
  const float keys = key_push(*binding, pad);
  return keys != 0.0f ? keys : stick_push(*binding, pad);
}

std::optional<bool> InputState::action_down(const std::string& name, std::size_t pad) const {
  const ActionBinding* binding = bindings_.action(pad, name);
  if (binding == nullptr) return std::nullopt;
  return held(current_, gamepads_.at(pad), *binding);
}

std::optional<bool> InputState::action_pressed(const std::string& name, std::size_t pad) const {
  const ActionBinding* binding = bindings_.action(pad, name);
  if (binding == nullptr) return std::nullopt;
  return just_pressed(*binding, pad);
}

std::optional<bool> InputState::action_released(const std::string& name, std::size_t pad) const {
  const ActionBinding* binding = bindings_.action(pad, name);
  if (binding == nullptr) return std::nullopt;
  const bool held_before = held(previous_, previous_gamepads_.at(pad), *binding);
  return held_before && !held(current_, gamepads_.at(pad), *binding);
}

}  // namespace redgem
