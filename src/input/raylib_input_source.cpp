#include "input/raylib_input_source.hpp"

#include <raylib.h>

namespace redgem {

bool RaylibInputSource::is_key_down(Key key) const { return IsKeyDown(static_cast<int>(key)); }

}  // namespace redgem
