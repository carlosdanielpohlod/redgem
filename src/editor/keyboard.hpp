#pragma once

// What the keyboard says right now, as the editor's shortcuts and tools read it.

#include <raylib.h>

#include "editor/editor_camera.hpp"
#include "editor/editor_fly_bindings.hpp"

namespace redgem::keyboard {

inline bool ctrl_down() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }
inline bool shift_down() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
inline bool alt_down() { return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); }

// The fly camera's keys, as the keyboard has them now.
inline FlyKeys fly_keys(const FlyBindings& bindings) {
  FlyKeys keys;
  keys.forward = IsKeyDown(static_cast<int>(bindings.forward));
  keys.back = IsKeyDown(static_cast<int>(bindings.back));
  keys.left = IsKeyDown(static_cast<int>(bindings.left));
  keys.right = IsKeyDown(static_cast<int>(bindings.right));
  keys.up = IsKeyDown(static_cast<int>(bindings.up));
  keys.down = IsKeyDown(static_cast<int>(bindings.down));
  keys.left_shift = IsKeyDown(KEY_LEFT_SHIFT);
  keys.right_shift = IsKeyDown(KEY_RIGHT_SHIFT);
  return keys;
}

}  // namespace redgem::keyboard
