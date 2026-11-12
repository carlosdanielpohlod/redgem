#pragma once

// One event of the input `redgem run --input` replays: what is held from its frame on (keys, mouse
// buttons, gamepads, a finger on the screen), and what lasts that frame only (a click, a tap, a
// turn of the wheel). It is fed to the ScriptedInputSource the game reads, as a player's would be.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "input/input_source.hpp"

namespace redgem::tools {

// A point on the screen: in pixels from its top-left corner, or, normalized, as fractions (0 to 1)
// of its width and height.
struct RunPoint {
  float x = 0.0f;
  float y = 0.0f;
  bool normalized = false;
};

struct RunMouse {
  std::optional<RunPoint> at;        // where it moves to (it stays where it was otherwise)
  std::vector<MouseButton> buttons;  // held until the next event
  std::vector<MouseButton> clicks;   // down for this frame, up on the next
  float wheel = 0.0f;                // notches turned this frame (up is positive)
};

struct RunPadAxis {
  GamepadAxis axis = GamepadAxis::LeftX;
  float value = 0.0f;
};

// A gamepad plugged in until the next event, with these buttons held and axes pushed.
struct RunPad {
  std::size_t pad = 0;
  std::vector<GamepadButton> buttons;
  std::vector<RunPadAxis> axes;
};

// A finger on a touch screen, which the game reads as its pointer (the mouse, a touch being its
// left button): down until the next event, or, a tap, for this frame only.
struct RunTouch {
  RunPoint at;
  bool tap = false;
};

// At a zero-based frame, replace everything held. Between events it stays held.
struct RunInput {
  long long frame = 0;
  std::vector<Key> keys;
  std::optional<RunMouse> mouse;
  std::vector<RunPad> pads;
  std::optional<RunTouch> touch;
};

// The size of the screen normalized points are fractions of, in pixels.
struct RunScreen {
  float width = 0.0f;
  float height = 0.0f;
};

// Reads one event of a run of `frames` frames into `event`; why it is not one otherwise.
std::string read_run_input(const nlohmann::json& value, long long frames, RunInput& event);

// Makes `event` what `source` holds, in place of whatever it held.
void apply_run_input(ScriptedInputSource& source, const RunInput& event, RunScreen screen);
// Lets go of what `event` held for its frame only (clicks, a tap, the wheel), the frame after it.
void end_run_pulses(ScriptedInputSource& source, const RunInput& event);

}  // namespace redgem::tools
