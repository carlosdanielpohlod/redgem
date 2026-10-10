#pragma once

#include "input/input.hpp"

namespace redgem {

// Reads the real keyboard through raylib. Requires an open window.
class RaylibInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override;
};

}  // namespace redgem
