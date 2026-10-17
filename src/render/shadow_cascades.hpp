#pragma once

// Cascaded sun shadows (Environment::shadow_cascades): the stretch of the view the sun's shadows
// reach is cut into slices along the camera's view, nearer ones thinner, and each slice gets a
// tile of the shadow map. A tile covers the sphere around its slice, whose size does not change as
// the camera turns, and moves in whole texels only: shadow edges stay still. One cascade is the
// single map of capture_views.hpp. No window and no graphics card needed, so it can be tested.

#include <raylib.h>

#include <array>

#include "core/math.hpp"
#include "render/capture_views.hpp"

namespace redgem::render_math {

inline constexpr int kMaxShadowCascades = 4;  // MAX_CASCADES in the lit shader
// How the slices are cut: 0 evenly, 1 each a constant factor deeper than the last.
inline constexpr float kCascadeSplitBlend = 0.75f;
// Meters above the sphere a cascade covers where what casts a shadow into it still counts.
inline constexpr float kCastersAbove = 100.0f;

// How far each slice reaches, meters from the camera: the last at `distance`.
using CascadeEnds = std::array<float, kMaxShadowCascades>;
CascadeEnds cascade_ends(int count, float near, float distance);

// The camera, as the cascades see it.
struct CascadeCamera {
  Vector3 position;
  Vector3 target;             // what it looks at
  Vector3 forward;            // unit length, toward the target
  float tan_half_fov = 0.0f;  // vertical
  float aspect = 1.0f;        // width over height
  float near = 0.0f;          // meters
};

// The sphere around the slice of the view from `from` to `to` meters: how far along the view its
// center is, and its radius.
struct SliceSphere {
  float center = 0.0f;
  float radius = 0.0f;
};
SliceSphere slice_sphere(const CascadeCamera& camera, float from, float to);

// One cascade: the sun's view of it, the meters it covers across, and its tile of the map (corner
// and size, in the map's texture coordinates).
struct Cascade {
  SunView view;
  float extent = 0.0f;
  Vector2 corner{};
  float size = 1.0f;
};
struct Cascades {
  std::array<Cascade, kMaxShadowCascades> cascades{};
  int count = 0;
  // Meters back each cascade's sun view sits: at least kCastersAbove beyond the largest cascade's
  // sphere. The sun's view holds depth up to twice that.
  float sun_distance = kShadowSunDistance;
};
// One cascade covers the map's single square (as without cascades); more share it, a quarter each,
// with the sun far enough back to hold the farthest slice (see Cascades::sun_distance).
Cascades fit_cascades(const CascadeCamera& camera, Vec3 to_sun, int count, float distance);

// The texels of one cascade's tile along a side.
int cascade_texels(int count);
// How much larger a texel of a cascade is than one of the single map.
float cascade_scale(const Cascade& cascade, int count);

}  // namespace redgem::render_math
