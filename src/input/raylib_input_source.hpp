#pragma once

#include "input/input.hpp"

namespace redgem {

// Reads the real keyboard and mouse through raylib. Requires an open window.
class RaylibInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override;
  MouseSample mouse() const override;
  GamepadSample gamepad() const override;
  std::string typed_text() const override;
};

}  // namespace redgem
