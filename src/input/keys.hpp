#pragma once

#include <optional>
#include <string>
#include <vector>

namespace redgem {

// Engine key codes. Values match raylib's KeyboardKey so the raylib source can cast directly.
enum class Key : int {
  Space = 32,
  Apostrophe = 39,
  Comma = 44,
  Minus = 45,
  Period = 46,
  Slash = 47,
  Zero = 48, One, Two, Three, Four, Five, Six, Seven, Eight, Nine,
  A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
  Escape = 256,
  Enter = 257,
  Tab = 258,
  Backspace = 259,
  Right = 262,
  Left = 263,
  Down = 264,
  Up = 265,
  LeftShift = 340,
  LeftControl = 341,
  LeftAlt = 342,
  RightShift = 344,
  RightControl = 345,
};

// Ruby-facing name ("a", "space", "left_shift") to key.
std::optional<Key> key_from_name(const std::string& name);
const std::vector<Key>& all_keys();

}  // namespace redgem
