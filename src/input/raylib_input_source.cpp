#include "input/raylib_input_source.hpp"

#include <raylib.h>

namespace redgem {

namespace {
constexpr float kHalf = 0.5f;
constexpr float kTriggerReleased = -1.0f;  // what raylib reports for a trigger nobody touches
// raylib's names for the axes and buttons, in GamepadAxis' and GamepadButton's order.
constexpr int kRaylibAxes[] = {GAMEPAD_AXIS_LEFT_X,  GAMEPAD_AXIS_LEFT_Y,       GAMEPAD_AXIS_RIGHT_X,
                               GAMEPAD_AXIS_RIGHT_Y, GAMEPAD_AXIS_LEFT_TRIGGER, GAMEPAD_AXIS_RIGHT_TRIGGER};
constexpr int kRaylibButtons[] = {
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
    GAMEPAD_BUTTON_RIGHT_FACE_UP,   GAMEPAD_BUTTON_LEFT_TRIGGER_1,   GAMEPAD_BUTTON_RIGHT_TRIGGER_1,
    GAMEPAD_BUTTON_MIDDLE_LEFT,     GAMEPAD_BUTTON_MIDDLE_RIGHT,     GAMEPAD_BUTTON_LEFT_FACE_UP,
    GAMEPAD_BUTTON_LEFT_FACE_DOWN,  GAMEPAD_BUTTON_LEFT_FACE_LEFT,   GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
    GAMEPAD_BUTTON_LEFT_THUMB,      GAMEPAD_BUTTON_RIGHT_THUMB};

// A gamepad, which must be plugged in.
GamepadSample read_gamepad(int pad) {
  GamepadSample sample;
  sample.connected = true;
  for (std::size_t i = 0; i < kGamepadAxisCount; ++i) {
    sample.axes[i] = GetGamepadAxisMovement(pad, kRaylibAxes[i]);
  }
  // raylib reports a trigger from -1 (released) to 1 (pulled): here 0 to 1.
  for (const GamepadAxis trigger : {GamepadAxis::LeftTrigger, GamepadAxis::RightTrigger}) {
    float& value = sample.axes[static_cast<std::size_t>(trigger)];
    value = (value - kTriggerReleased) * kHalf;
  }
  for (std::size_t i = 0; i < kGamepadButtonCount; ++i) {
    sample.buttons[i] = IsGamepadButtonDown(pad, kRaylibButtons[i]);
  }
  return sample;
}

}  // namespace

bool RaylibInputSource::is_key_down(Key key) const { return IsKeyDown(static_cast<int>(key)); }

MouseSample RaylibInputSource::mouse() const {
  MouseSample sample;
  const Vector2 position = GetMousePosition();
  sample.x = position.x;
  sample.y = position.y;
  sample.wheel = GetMouseWheelMove();
  sample.buttons = {IsMouseButtonDown(MOUSE_BUTTON_LEFT), IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
                    IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)};
  return sample;
}

std::string RaylibInputSource::typed_text() const {
  std::string text;
  // raylib queues the characters typed; 0 means the queue is empty.
  for (int codepoint = GetCharPressed(); codepoint != 0; codepoint = GetCharPressed()) {
    int bytes = 0;
    const char* encoded = CodepointToUTF8(codepoint, &bytes);
    text.append(encoded, static_cast<std::size_t>(bytes));
  }
  return text;
}

GamepadSample RaylibInputSource::gamepad(std::size_t pad) const {
  const int number = static_cast<int>(pad);
  return IsGamepadAvailable(number) ? read_gamepad(number) : GamepadSample{};
}

}  // namespace redgem
