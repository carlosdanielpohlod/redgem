#pragma once

// How a sky picture is laid around the world, and the cube map its light is kept in. The picture
// is equirectangular (longitude across, latitude down, the zenith along its top row) and is laid
// as Blender lays a world's picture: its middle looks along +X, and turning right across it turns
// toward +Z. The lit and sky shaders read it the same way (see shaders/sky_light.hpp). No window
// and no graphics card needed, so it can be tested.

#include "core/float_image.hpp"
#include "core/math.hpp"

namespace redgem::render_math {

inline constexpr int kCubeMapFaces = 6;  // +X, -X, +Y, -Y, +Z, -Z: the graphics card's order

// A place on the picture: 0..1 from its left, 0..1 from its top.
struct PictureSpot {
  float u = 0.0f;
  float v = 0.0f;
};
Vec3 picture_direction(PictureSpot spot);
PictureSpot picture_spot(Vec3 direction);

// `direction` turned by `radians` around the vertical (the way a sky's rotation turns it).
Vec3 turned_around_up(Vec3 direction, float radians);

// The direction through texture coordinates (s, t) of a face of a cube map, both 0..1, t
// counting from the face's first row as the graphics card lays it.
Vec3 cube_direction(int face, float s, float t);

// The picture's light in a direction, filtered between its four nearest pixels (it wraps around
// the horizon, and stops at the poles).
Vec3 picture_light(const FloatImage& picture, Vec3 direction);

}  // namespace redgem::render_math
