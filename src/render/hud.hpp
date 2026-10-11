#pragma once

// What is drawn flat over the game, in pixels from the top-left corner: plain descriptions, made
// by whoever has something to show (scripts, the touch controls) and drawn by the renderer.

#include <string>

#include "scene/scene_desc.hpp"

namespace redgem {

// One thing a script draws over the game this frame (UI.text, UI.rect, UI.image).
struct HudItem {
  enum class Kind { Text, Rect, Image };
  Kind kind = Kind::Text;
  std::string text;  // the text, or the image's name (a file in the game's images/ folder)
  int x = 0;
  int y = 0;
  int width = 0;  // rectangles and images
  int height = 0;
  int size = 0;         // text height
  float radius = 0.0f;  // rounded corners of a rectangle, in pixels
  Color color;
};

// One of the controls a touch screen shows, as a see-through circle.
struct ControlCircle {
  // How much it stands out: the ring a stick moves in is always faint; a button is faint until a
  // finger is on it.
  enum class Look { Base, Resting, Pressed };
  float x = 0.0f;
  float y = 0.0f;
  float radius = 0.0f;
  Look look = Look::Resting;
};

}  // namespace redgem
