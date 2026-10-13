#pragma once

// What the keyboard says right now, as the editor's shortcuts and tools read it.

#include <raylib.h>

#include "editor/editor_camera.hpp"

namespace redgem::keyboard {

inline bool ctrl_down() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }
inline bool shift_down() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
inline bool alt_down() { return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); }

// The fly camera's keys, as the keyboard has them now.
inline FlyKeys fly_keys() {
  FlyKeys keys;
  keys.forward = IsKeyDown(KEY_W);
  keys.back = IsKeyDown(KEY_S);
  keys.left = IsKeyDown(KEY_A);
  keys.right = IsKeyDown(KEY_D);
  keys.up = IsKeyDown(KEY_E);
  keys.down = IsKeyDown(KEY_Q);
  keys.left_shift = IsKeyDown(KEY_LEFT_SHIFT);
  keys.right_shift = IsKeyDown(KEY_RIGHT_SHIFT);
  return keys;
}

}  // namespace redgem::keyboard
