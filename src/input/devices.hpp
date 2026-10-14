#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

// A mouse and a gamepad as the engine sees them: their buttons and axes, by the names scripts
// call them, and what each holds at one instant.
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

// A gamepad's sticks and triggers, and its buttons (named as on an Xbox pad: A is the bottom one).
enum class GamepadAxis { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger };
enum class GamepadButton {
  A,
  B,
  X,
  Y,
  LeftBumper,
  RightBumper,
  Back,
  Start,
  DpadUp,
  DpadDown,
  DpadLeft,
  DpadRight,
  LeftStick,
  RightStick
};
inline constexpr std::size_t kGamepadAxisCount = 6;
inline constexpr std::size_t kGamepadButtonCount = 14;

// "left_x" -> GamepadAxis::LeftX, "a" -> GamepadButton::A; nullopt for an unknown name.
std::optional<GamepadAxis> gamepad_axis_from_name(const std::string& name);
std::optional<GamepadButton> gamepad_button_from_name(const std::string& name);

// The first gamepad at one instant. Sticks go from -1 to 1 (left and up are negative), triggers
// from 0 to 1.
struct GamepadSample {
  bool connected = false;
  std::array<float, kGamepadAxisCount> axes{};
  std::array<bool, kGamepadButtonCount> buttons{};
};

}  // namespace redgem
