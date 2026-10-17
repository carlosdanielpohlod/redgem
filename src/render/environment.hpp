#pragma once

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
  // The brightest that glowing particles together make a pixel, in linear light: many of them over
  // each other come near it instead of washing out to white (0 = no limit).
  float glow_limit = 0.0f;
};

// A clear day: what scenes without an environment object get.
Environment default_environment();

// The defaults with whatever the props override:
//   sky_top, sky_horizon, ground_color, fog_color   "#rrggbb"
//   fog_density, ambient, exposure, bloom, vignette, sun_glow, wind, shadows, ambient_occlusion,
//   contrast, saturation, grain, aberration, glow_limit   numbers
Environment environment_from_props(const PropertyMap& props);

// The environment of the first live object with `environment = true`, or the default.
Environment scene_environment(const Scene& scene);

}  // namespace redgem
