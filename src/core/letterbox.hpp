#pragma once

#include <algorithm>

#include "core/math.hpp"

namespace redgem {

// Where a picture is shown in a window of another size: as large as fits, in its middle (bars fill
// the rest). The window shows a picture drawn at another size so (a render scale, a recording),
// and the interface finds the world's points on it the same way (Camera.world_to_screen).
struct Letterbox {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

inline Letterbox fit_picture(float window_width, float window_height, float picture_width, float picture_height) {
  const float scale = std::min(window_width / picture_width, window_height / picture_height);
  const float width = picture_width * scale;
  const float height = picture_height * scale;
  return {(window_width - width) * kHalf, (window_height - height) * kHalf, width, height};
}

}  // namespace redgem
