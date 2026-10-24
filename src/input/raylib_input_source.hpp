#pragma once

#include "input/input.hpp"

namespace redgem {

// Reads the real keyboard, mouse and gamepad through raylib. Requires an open window. (A touch
// screen's fingers are read by TouchInputSource, which a program for a touch screen puts in front
// of this one.)
class RaylibInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override;
  MouseSample mouse() const override;
  GamepadSample gamepad(std::size_t pad) const override;
  std::string typed_text() const override;
};

}  // namespace redgem
