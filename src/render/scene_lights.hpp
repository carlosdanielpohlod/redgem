#pragma once

// The lights a frame is drawn with: the scene's sun, and the point lights that matter to a viewer.

#include <cstddef>
#include <vector>

#include "core/math.hpp"
#include "scene/scene.hpp"

namespace redgem {

// A point light candidate for a frame.
struct PointLight {
  Vec3 position;
  Vec3 color;  // linear, already scaled by intensity
  float range = 0.0f;
};

// The lights that matter most to a viewer: those whose lit sphere is closest to `viewer`,
// nearest first, at most `max_count`. When some are left out, the farthest kept ones are dimmed
// over the last `fade_distance` meters before the cut, so lights fade in and out as the viewer
// moves instead of popping.
std::vector<PointLight> nearest_lights(std::vector<PointLight> lights, Vec3 viewer, std::size_t max_count,
                                       float fade_distance = 0.0f);

// The lights of a scene as the renderer uses them: one sun and every point light.
struct SceneLights {
  Vec3 sun_direction;  // the way the sun's light travels
  Vec3 sun_color;      // linear, already scaled by intensity
  std::vector<PointLight> points;
};

// The first directional light is the sun (a default one when the scene has none); every other
// light shines from a point. Lights of prefabs do not count.
SceneLights scene_lights(const Scene& scene);

}  // namespace redgem
