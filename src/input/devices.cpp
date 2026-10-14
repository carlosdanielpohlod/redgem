#include "input/devices.hpp"

namespace redgem {
namespace {

// What Ruby calls something, and what it is.
template <typename Value>
struct Named {
  const char* name = nullptr;
  Value value{};
};

constexpr Named<GamepadAxis> kGamepadAxisNames[] = {
    {"left_x", GamepadAxis::LeftX},
    {"left_y", GamepadAxis::LeftY},
    {"right_x", GamepadAxis::RightX},
    {"right_y", GamepadAxis::RightY},
    {"left_trigger", GamepadAxis::LeftTrigger},
    {"right_trigger", GamepadAxis::RightTrigger},
};

constexpr Named<GamepadButton> kGamepadButtonNames[] = {
    {"a", GamepadButton::A},
    {"b", GamepadButton::B},
    {"x", GamepadButton::X},
    {"y", GamepadButton::Y},
    {"left_bumper", GamepadButton::LeftBumper},
    {"right_bumper", GamepadButton::RightBumper},
    {"back", GamepadButton::Back},
    {"start", GamepadButton::Start},
    {"dpad_up", GamepadButton::DpadUp},
    {"dpad_down", GamepadButton::DpadDown},
    {"dpad_left", GamepadButton::DpadLeft},
    {"dpad_right", GamepadButton::DpadRight},
    {"left_stick", GamepadButton::LeftStick},
    {"right_stick", GamepadButton::RightStick},
};

constexpr Named<MouseButton> kMouseButtonNames[] = {
    {"left", MouseButton::Left}, {"right", MouseButton::Right}, {"middle", MouseButton::Middle}};

template <typename Value, std::size_t Count>
std::optional<Value> find_named(const Named<Value> (&table)[Count], const std::string& name) {
  for (const Named<Value>& known : table) {
    if (name == known.name) return known.value;
  }
  return std::nullopt;
}

}  // namespace

std::optional<GamepadAxis> gamepad_axis_from_name(const std::string& name) {
  return find_named(kGamepadAxisNames, name);
}

std::optional<GamepadButton> gamepad_button_from_name(const std::string& name) {
  return find_named(kGamepadButtonNames, name);
}

std::optional<MouseButton> mouse_button_from_name(const std::string& name) {
  return find_named(kMouseButtonNames, name);
}

}  // namespace redgem
