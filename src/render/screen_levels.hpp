#pragma once

// Levels of detail chosen by how big an object looks (GameObject::screen_level): the sizes on
// screen turned into the distances it is drawn at, for the view's field of view. Pure: no window
// needed, so it can be tested.

#include <limits>

#include "scene/scene.hpp"

namespace redgem::render_math {

// The distances from the camera an object is drawn at, in meters, and over how many of them it
// fades out before `to` (and in after a `from` above 0) through the dither; 0: it does not fade.
struct DrawnRange {
  float from = 0.0f;
  float to = std::numeric_limits<float>::infinity();
  float fade = 0.0f;
};

// An object's own range (`visible_from`, `visible_to`) narrowed to where its level of detail is
// drawn. `scale` is the longest axis of its world scale; `tan_half_fov` the tangent of half the
// view's vertical field of view. The level that replaces it farther away starts `fade` meters
// before it ends, so the two cross over with complementary pixels.
DrawnRange drawn_range(const GameObject& object, float scale, float tan_half_fov);

// The longest axis of a scale (a mirrored one counts as long as its size).
float longest_axis(Vec3 scale);

}  // namespace redgem::render_math
