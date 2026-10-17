#pragma once

// The views a frame captures before its picture: the sun's, for the shadow map, and one face of
// a cube around the camera, for the reflections. No window and no graphics card needed, so it can
// be tested.

#include <raylib.h>

#include "core/math.hpp"

namespace redgem::render_math {

// Sun shadows: one map covering a square of the world around where the camera looks.
inline constexpr int kShadowMapSize = 4096;    // texels along each side
inline constexpr float kShadowExtent = 90.0f;  // meters covered by the map (side of the square)
inline constexpr float kShadowFocusAhead =
    0.3f;  // the square is centered this fraction of its side ahead of the camera
inline constexpr float kShadowSunDistance =
    300.0f;  // meters back the sun's viewpoint sits (casters behind the camera count)
inline constexpr float kMinSunHeight =
    0.02f;  // below this (y of the direction to the sun) it is treated as set: no shadows
inline constexpr float kNoShadows = 0.0f;
inline constexpr float kNearlyVertical = 0.999f;  // |y| of a sun direction beyond which Y cannot be its "up"
// Reflections: the surroundings of the camera, captured one cube face per frame.
inline constexpr int kCubeFaces = 6;
inline constexpr float kReflectionEyeHeight = 1.2f;  // lowest world height (y, meters) the capture is taken from
inline constexpr float kReflectionsOn = 1.0f;
inline constexpr float kReflectionsOff = 0.0f;

// --- Sun shadows -----------------------------------------------------------------------------

// How dark the sun's shadows are: `asked`, or none once the sun has set.
float sun_shadow_strength(Vec3 to_sun, float asked);
// `value` moved down to a whole number of `step`s.
float snapped_down(float value, float step);
// The axis the shadow map's vertical follows. A sun straight overhead has no sideways "up":
// another axis gives the map its orientation.
Vector3 shadow_map_vertical(Vector3 sun_axis);
// The sun's viewpoint for the shadow map.
struct SunView {
  Vector3 position;
  Vector3 target;
  Vector3 up;
};
// Looks at a point ahead of the camera, moved in whole texels only: otherwise shadow edges
// shimmer as the camera moves.
SunView fit_sun_view(Vector3 camera_position, Vector3 camera_target, Vec3 to_sun);
// The sun looking at `focus` from `distance` meters away, moved across its view to a whole number
// of `texel`s (meters).
SunView snapped_sun_view(Vector3 focus, Vec3 to_sun, float texel, float distance = kShadowSunDistance);

// --- Reflections -----------------------------------------------------------------------------

// The view along one face of a cube around the camera.
struct CubeFace {
  Vector3 forward;
  Vector3 up;
};
inline constexpr CubeFace kCubeFaceViews[kCubeFaces] = {
    {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, {{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, {{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
};
// The right and up of a face's picture, at right angles to its forward.
struct FaceAxes {
  Vector3 right;
  Vector3 up;
};
FaceAxes face_axes(const CubeFace& face);
// Where the surroundings are captured from: the camera, but not from below a height (a camera
// near the floor would mostly capture the floor).
Vector3 reflection_eye(Vector3 camera_position);
// The face captured after `face`: six frames make a full turn.
int face_after(int face);
// Reflections switch on once all six faces exist.
float reflection_strength(int faces_captured);

}  // namespace redgem::render_math
