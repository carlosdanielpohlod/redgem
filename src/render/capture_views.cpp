#include "render/capture_views.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>

#include "render/render_math.hpp"

namespace redgem::render_math {

// --- Sun shadows -----------------------------------------------------------------------------

float sun_shadow_strength(Vec3 to_sun, float asked) { return to_sun.y > kMinSunHeight ? asked : kNoShadows; }

float snapped_down(float value, float step) { return std::floor(value / step) * step; }

Vector3 shadow_map_vertical(Vector3 sun_axis) {
  return std::fabs(sun_axis.y) > kNearlyVertical ? kForwardAxis : kUpAxis;
}

SunView fit_sun_view(Vector3 camera_position, Vector3 camera_target, Vec3 to_sun) {
  const Vector3 forward = Vector3Normalize(Vector3Subtract(camera_target, camera_position));
  const Vector3 ahead = Vector3Add(camera_position, Vector3Scale(forward, kShadowExtent * kShadowFocusAhead));
  return snapped_sun_view(ahead, to_sun, kShadowExtent / static_cast<float>(kShadowMapSize));
}

SunView snapped_sun_view(Vector3 ahead, Vec3 to_sun, float texel_size, float distance) {
  const Vector3 sun_axis = to_raylib(to_sun);
  const Vector3 map_vertical = shadow_map_vertical(sun_axis);
  const Vector3 map_right = Vector3Normalize(Vector3CrossProduct(map_vertical, sun_axis));
  const Vector3 map_up = Vector3CrossProduct(sun_axis, map_right);
  // Snapped across the map; along the sun's own axis the place does not matter to the texels.
  const Vector3 across = Vector3Scale(map_right, snapped_down(Vector3DotProduct(ahead, map_right), texel_size));
  const Vector3 upward = Vector3Scale(map_up, snapped_down(Vector3DotProduct(ahead, map_up), texel_size));
  const Vector3 focus =
      Vector3Add(Vector3Add(across, upward), Vector3Scale(sun_axis, Vector3DotProduct(ahead, sun_axis)));
  return {Vector3Add(focus, Vector3Scale(sun_axis, distance)), focus, map_vertical};
}

// --- Reflections -----------------------------------------------------------------------------

FaceAxes face_axes(const CubeFace& face) {
  const Vector3 right = Vector3Normalize(Vector3CrossProduct(face.forward, face.up));
  return {right, Vector3CrossProduct(right, face.forward)};
}

Vector3 reflection_eye(Vector3 camera_position) {
  return {camera_position.x, std::max(camera_position.y, kReflectionEyeHeight), camera_position.z};
}

int face_after(int face) { return (face + 1) % kCubeFaces; }

float reflection_strength(int faces_captured) {
  return faces_captured >= kCubeFaces ? kReflectionsOn : kReflectionsOff;
}

}  // namespace redgem::render_math
