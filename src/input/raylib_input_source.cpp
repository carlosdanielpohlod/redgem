#include "input/raylib_input_source.hpp"

#include <raylib.h>

namespace redgem {

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

}  // namespace redgem
