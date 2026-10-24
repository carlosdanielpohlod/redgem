#include "window_mouse.hpp"

#include <raylib.h>

namespace redgem::test {

void move_window_mouse(float x, float y) { SetMousePosition(static_cast<int>(x), static_cast<int>(y)); }

}  // namespace redgem::test
