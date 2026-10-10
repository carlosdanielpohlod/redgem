#include "render/environment.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/config.hpp"

namespace redgem {
namespace {

constexpr const char* kEnvironmentProp = "environment";

// A clear day (linear colors).
constexpr Vec3 kDaySkyTop{0.16f, 0.36f, 0.78f};
constexpr Vec3 kDaySkyHorizon{0.55f, 0.72f, 0.92f};
constexpr Vec3 kDayGround{0.16f, 0.14f, 0.12f};
constexpr float kDayFogDensity = 0.004f;
constexpr float kDayAmbient = 0.5f;
constexpr float kDayExposure = 1.0f;
constexpr float kDayBloom = 0.35f;
constexpr float kDayVignette = 0.25f;
constexpr float kDaySunGlow = 1.0f;
constexpr float kDayWind = 0.4f;
constexpr float kDayShadows = 1.0f;
constexpr float kDayAmbientOcclusion = 0.8f;
constexpr float kFullShadows = 1.0f;  // the most a 0..1 strength (shadows, occlusion, vignette) can be

void read_color(const PropertyMap& props, const char* key, Vec3& out) {
  if (const auto text = get_string(props, key)) {
    if (const auto color = parse_hex_color(*text)) out = *color;
  }
}

void read_number(const PropertyMap& props, const char* key, float& out) {
  if (const auto value = get_number(props, key)) out = std::max(static_cast<float>(*value), 0.0f);
}

}  // namespace

Environment default_environment() {
  Environment env;
  env.sky_top = kDaySkyTop;
  env.sky_horizon = kDaySkyHorizon;
  env.ground = kDayGround;
  env.fog_color = kDaySkyHorizon;
  env.fog_density = kDayFogDensity;
  env.ambient = kDayAmbient;
  env.exposure = kDayExposure;
  env.bloom = kDayBloom;
  env.vignette = kDayVignette;
  env.sun_glow = kDaySunGlow;
  env.wind = kDayWind;
  env.shadows = kDayShadows;
  env.ambient_occlusion = kDayAmbientOcclusion;
  return env;
}

Environment environment_from_props(const PropertyMap& props) {
  Environment env = default_environment();
  read_color(props, "sky_top", env.sky_top);
  read_color(props, "sky_horizon", env.sky_horizon);
  read_color(props, "ground_color", env.ground);
  env.fog_color = env.sky_horizon;  // fog fades into the horizon unless given its own color
  read_color(props, "fog_color", env.fog_color);
  read_number(props, "fog_density", env.fog_density);
  read_number(props, "ambient", env.ambient);
  read_number(props, "exposure", env.exposure);
  read_number(props, "bloom", env.bloom);
  read_number(props, "vignette", env.vignette);
  env.vignette = std::min(env.vignette, kFullShadows);
  read_number(props, "sun_glow", env.sun_glow);
  read_number(props, "wind", env.wind);
  read_number(props, "shadows", env.shadows);
  env.shadows = std::min(env.shadows, kFullShadows);
  read_number(props, "ambient_occlusion", env.ambient_occlusion);
  env.ambient_occlusion = std::min(env.ambient_occlusion, kFullShadows);
  return env;
}

Environment scene_environment(const Scene& scene) {
  for (const ObjectId id : scene.alive_ids()) {
    const PropertyMap& props = scene.at(id).props;
    if (get_bool(props, kEnvironmentProp).value_or(false)) return environment_from_props(props);
  }
  return default_environment();
}

std::vector<PointLight> nearest_lights(std::vector<PointLight> lights, Vec3 viewer, std::size_t max_count,
                                       float fade_distance) {
  // Distance to the edge of what the light reaches: a far but wide light can matter more than a
  // near but tiny one.
  const auto reach = [viewer](const PointLight& light) { return length(light.position - viewer) - light.range; };
  std::stable_sort(lights.begin(), lights.end(),
                   [&reach](const PointLight& a, const PointLight& b) { return reach(a) < reach(b); });
  if (lights.size() <= max_count) return lights;
  // The first light left out marks the cut: kept lights fade to nothing as they approach it.
  const float cut = reach(lights[max_count]);
  lights.resize(max_count);
  if (fade_distance > 0.0f) {
    for (PointLight& light : lights) {
      light.color = light.color * std::clamp((cut - reach(light)) / fade_distance, 0.0f, 1.0f);
    }
  }
  return lights;
}

}  // namespace redgem
