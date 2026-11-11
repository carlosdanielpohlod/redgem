#pragma once

// The lights a frame is drawn with: the scene's sun, and the point and spot lights that matter to
// a viewer.

#include <cstddef>
#include <vector>

#include "core/math.hpp"
#include "scene/scene.hpp"

namespace redgem {

// How a spot light dims away from its axis: the light a direction gets is
// clamp(dot(direction, axis) * scale + offset, 0, 1) squared (glTF KHR_lights_punctual). A point
// light's (scale 0, offset 1) shines the same every way.
struct SpotCone {
  float scale = 0.0f;
  float offset = 1.0f;
};
// The cone of a spot whose light starts dimming `inner` radians off its axis and is gone at
// `outer` (held to a cone that opens forward, inner inside outer).
SpotCone spot_cone(float inner, float outer);

// A point or spot light candidate for a frame.
struct PointLight {
  Vec3 position;
  Vec3 color;  // linear, already scaled by intensity
  float range = 0.0f;
  bool spot = false;
  Vec3 axis = Vec3::zero();  // spots: the way their light travels (unit length)
  SpotCone cone;
  float outer_cone = 0.0f;   // spots: radians from the axis to where the light is gone
  bool cast_shadow = false;  // prop `cast_shadow` on the light's object (off unless said)
};

// The lights that matter most to a viewer: those whose lit sphere is closest to `viewer`,
// nearest first, at most `max_count`. When some are left out, the farthest kept ones are dimmed
// over the last `fade_distance` meters before the cut, so lights fade in and out as the viewer
// moves instead of popping.
std::vector<PointLight> nearest_lights(std::vector<PointLight> lights, Vec3 viewer, std::size_t max_count,
                                       float fade_distance = 0.0f);

// The lights of a scene as the renderer uses them: one sun and every point and spot light.
struct SceneLights {
  Vec3 sun_direction;  // the way the sun's light travels
  Vec3 sun_color;      // linear, already scaled by intensity
  std::vector<PointLight> points;
};

// The first directional light is the sun (a default one when the scene has none); every other
// light shines from a point, spots within their cone. Lights of prefabs do not count.
SceneLights scene_lights(const Scene& scene);

}  // namespace redgem
