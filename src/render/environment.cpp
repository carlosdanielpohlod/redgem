#include "render/environment.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/config.hpp"

namespace redgem {
namespace {

// The custom properties of an environment object.
constexpr const char* kEnvironmentProp = "environment";
constexpr const char* kSkyTopProp = "sky_top";
constexpr const char* kSkyHorizonProp = "sky_horizon";
constexpr const char* kGroundColorProp = "ground_color";
constexpr const char* kFogColorProp = "fog_color";
constexpr const char* kFogDensityProp = "fog_density";
constexpr const char* kAmbientProp = "ambient";
constexpr const char* kExposureProp = "exposure";
constexpr const char* kBloomProp = "bloom";
constexpr const char* kVignetteProp = "vignette";
constexpr const char* kSunGlowProp = "sun_glow";
constexpr const char* kWindProp = "wind";
constexpr const char* kShadowsProp = "shadows";
constexpr const char* kAmbientOcclusionProp = "ambient_occlusion";
constexpr const char* kContrastProp = "contrast";
constexpr const char* kSaturationProp = "saturation";
constexpr const char* kGrainProp = "grain";
constexpr const char* kAberrationProp = "aberration";

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
constexpr float kNeutralGrade = 1.0f;  // contrast and saturation that change nothing
constexpr float kFullShadows = 1.0f;   // the most a 0..1 strength (shadows, occlusion, vignette) can be

void read_color(const PropertyMap& props, const char* key, Vec3& out) {
  if (const auto text = get_string(props, key)) {
    if (const auto color = parse_hex_color(*text)) out = *color;
  }
}

void read_number(const PropertyMap& props, const char* key, float& out) {
  if (const auto value = get_number(props, key)) out = std::max(static_cast<float>(*value), 0.0f);
}

// A 0..1 strength: anything above full counts as full.
void read_strength(const PropertyMap& props, const char* key, float& out) {
  read_number(props, key, out);
  out = std::min(out, kFullShadows);
}

// The sun scenes without a directional light get.
SceneLights default_lights() {
  SceneLights lights;
  lights.sun_direction =
      normalize(Vec3{config::kDefaultSunDirectionX, config::kDefaultSunDirectionY, config::kDefaultSunDirectionZ});
  lights.sun_color = Vec3::one() * config::kDefaultSunIntensity;
  return lights;
}

PointLight point_light(const LightDesc& light, Vec3 position) {
  const Vec3 color{light.color.r, light.color.g, light.color.b};
  return {position, color * (light.intensity * config::kPointWattScale),
          light.range > 0.0f ? light.range : config::kDefaultPointLightRange};
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
  env.contrast = kNeutralGrade;
  env.saturation = kNeutralGrade;
  return env;
}

Environment environment_from_props(const PropertyMap& props) {
  Environment env = default_environment();
  read_color(props, kSkyTopProp, env.sky_top);
  read_color(props, kSkyHorizonProp, env.sky_horizon);
  read_color(props, kGroundColorProp, env.ground);
  env.fog_color = env.sky_horizon;  // fog fades into the horizon unless given its own color
  read_color(props, kFogColorProp, env.fog_color);
  read_number(props, kFogDensityProp, env.fog_density);
  read_number(props, kAmbientProp, env.ambient);
  read_number(props, kExposureProp, env.exposure);
  read_number(props, kBloomProp, env.bloom);
  read_strength(props, kVignetteProp, env.vignette);
  read_number(props, kSunGlowProp, env.sun_glow);
  read_number(props, kWindProp, env.wind);
  read_strength(props, kShadowsProp, env.shadows);
  read_strength(props, kAmbientOcclusionProp, env.ambient_occlusion);
  read_number(props, kContrastProp, env.contrast);
  read_number(props, kSaturationProp, env.saturation);
  read_number(props, kGrainProp, env.grain);
  read_number(props, kAberrationProp, env.aberration);
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

SceneLights scene_lights(const Scene& scene) {
  SceneLights lights = default_lights();
  bool sun_found = false;
  for (const ObjectId id : scene.alive_ids()) {
    const GameObject& object = scene.at(id);
    if (!object.light || object.prefab) continue;
    const LightDesc& light = *object.light;
    const Transform world = scene.world_transform(id);
    if (light.type != LightType::Directional) {
      lights.points.push_back(point_light(light, world.position));
      continue;
    }
    if (sun_found) continue;
    sun_found = true;
    lights.sun_direction = world.forward();
    lights.sun_color =
        Vec3{light.color.r, light.color.g, light.color.b} * (light.intensity * config::kSunStrengthScale);
  }
  return lights;
}

}  // namespace redgem
