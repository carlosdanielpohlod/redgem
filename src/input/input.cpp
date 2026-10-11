#include "input/input.hpp"

#include <cmath>

#include <algorithm>

namespace redgem {
namespace {

constexpr float kStickDeadZone = 0.2f;  // of a stick's travel
constexpr float kAxisNegative = -1.0f;
constexpr float kAxisPositive = 1.0f;

}  // namespace

void ScriptedInputSource::set_key(Key key, bool down) {
  if (down) {
    held_.insert(key);
  } else {
    held_.erase(key);
  }
}

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
}

float InputState::gamepad_axis(GamepadAxis axis) const {
  const float value = gamepad_.axes[static_cast<std::size_t>(axis)];
  const bool trigger = axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger;
  return !trigger && std::fabs(value) < kStickDeadZone ? 0.0f : value;
}

std::optional<GamepadAxis> gamepad_axis_from_name(const std::string& name) {
  static const std::map<std::string, GamepadAxis> names = {
      {"left_x", GamepadAxis::LeftX},   {"left_y", GamepadAxis::LeftY},
      {"right_x", GamepadAxis::RightX}, {"right_y", GamepadAxis::RightY},
      {"left_trigger", GamepadAxis::LeftTrigger}, {"right_trigger", GamepadAxis::RightTrigger}};
  const auto found = names.find(name);
  return found == names.end() ? std::nullopt : std::optional<GamepadAxis>(found->second);
}

std::optional<GamepadButton> gamepad_button_from_name(const std::string& name) {
  static const std::map<std::string, GamepadButton> names = {
      {"a", GamepadButton::A}, {"b", GamepadButton::B}, {"x", GamepadButton::X}, {"y", GamepadButton::Y},
      {"left_bumper", GamepadButton::LeftBumper}, {"right_bumper", GamepadButton::RightBumper},
      {"back", GamepadButton::Back}, {"start", GamepadButton::Start},
      {"dpad_up", GamepadButton::DpadUp}, {"dpad_down", GamepadButton::DpadDown},
      {"dpad_left", GamepadButton::DpadLeft}, {"dpad_right", GamepadButton::DpadRight},
      {"left_stick", GamepadButton::LeftStick}, {"right_stick", GamepadButton::RightStick}};
  const auto found = names.find(name);
  return found == names.end() ? std::nullopt : std::optional<GamepadButton>(found->second);
}

std::optional<MouseButton> mouse_button_from_name(const std::string& name) {
  if (name == "left") return MouseButton::Left;
  if (name == "right") return MouseButton::Right;
  if (name == "middle") return MouseButton::Middle;
  return std::nullopt;
}

std::optional<float> InputState::axis(const std::string& name) const {
  const AxisBinding* binding = map_.axis(name);
  if (binding == nullptr) return std::nullopt;
  auto any_down = [this](const std::vector<Key>& keys) {
    return std::any_of(keys.begin(), keys.end(), [this](Key key) { return down(key); });
  };
  float value = 0.0f;
  if (any_down(binding->negative)) value += kAxisNegative;
  if (any_down(binding->positive)) value += kAxisPositive;
  // The keyboard wins; with no key held, the stick decides (and by how much: it is not just on or off).
  if (value == 0.0f && binding->stick && gamepad_.connected) {
    const float pushed = gamepad_axis(*binding->stick);
    if (pushed != 0.0f) value = pushed * (binding->stick_inverted ? kAxisNegative : kAxisPositive);
  }
  return value;
}

std::optional<bool> InputState::action_down(const std::string& name) const {
  const ActionBinding* binding = map_.action(name);
  if (binding == nullptr) return std::nullopt;
  return std::any_of(binding->keys.begin(), binding->keys.end(), [this](Key key) { return down(key); }) ||
         std::any_of(binding->buttons.begin(), binding->buttons.end(),
                     [this](GamepadButton button) { return gamepad_down(button); });
}

std::optional<bool> InputState::action_pressed(const std::string& name) const {
  const ActionBinding* binding = map_.action(name);
  if (binding == nullptr) return std::nullopt;
  return std::any_of(binding->keys.begin(), binding->keys.end(), [this](Key key) { return pressed(key); }) ||
         std::any_of(binding->buttons.begin(), binding->buttons.end(),
                     [this](GamepadButton button) { return gamepad_pressed(button); });
}

}  // namespace redgem
