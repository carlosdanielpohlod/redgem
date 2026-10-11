#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "scene/property.hpp"
#include "scene/scene.hpp"

namespace redgem {

// How a scene looks beyond its objects: sky, fog, ambient light and post-processing.
// Colors are linear RGB.
struct Environment {
  Vec3 sky_top;      // straight up
  Vec3 sky_horizon;  // around the horizon (also what the sun glow tints)
  Vec3 ground;       // below the horizon (lights surfaces from below)
  Vec3 fog_color;
  float fog_density = 0.0f;        // exponential fog per meter
  float ambient = 0.0f;            // strength of the sky's light on surfaces
  float exposure = 0.0f;           // brightness multiplier before tone mapping
  float bloom = 0.0f;              // glow around bright things (0 = off)
  float vignette = 0.0f;           // darkening toward the corners (0 = off)
  float sun_glow = 0.0f;           // brightness of the sun's disc and halo in the sky
  float wind = 0.0f;               // how much foliage sways (0 = still)
  float shadows = 0.0f;            // darkness of the sun's shadows (0 = none, 1 = full)
  float ambient_occlusion = 0.0f;  // darkening of creases and contacts (0 = none, 1 = full)
  // The finished picture's grade, like a film's.
  float contrast = 0.0f;    // 1 leaves it as it is; above, darks get darker and lights lighter
  float saturation = 0.0f;  // 1 leaves colors as they are; 0 is black and white
  float grain = 0.0f;       // film grain (0 = none)
  float aberration = 0.0f;  // color fringes toward the corners, as through a lens (0 = none)
};

// A clear day: what scenes without an environment object get.
Environment default_environment();

// The defaults with whatever the props override:
//   sky_top, sky_horizon, ground_color, fog_color   "#rrggbb"
//   fog_density, ambient, exposure, bloom, vignette, sun_glow, wind, shadows, ambient_occlusion,
//   contrast, saturation, grain, aberration   numbers
Environment environment_from_props(const PropertyMap& props);

// The environment of the first live object with `environment = true`, or the default.
Environment scene_environment(const Scene& scene);

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
