#include "render/environment.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "render/render_config.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

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

// The properties that are numbers, and the part of the environment each one sets.
struct NumberProp {
  const char* key;
  float Environment::* value;
};
constexpr NumberProp kNumberProps[] = {
    {props::environment::kFogDensity, &Environment::fog_density},
    {props::environment::kAmbient, &Environment::ambient},
    {props::environment::kExposure, &Environment::exposure},
    {props::environment::kBloom, &Environment::bloom},
    {props::environment::kSunGlow, &Environment::sun_glow},
    {props::environment::kWind, &Environment::wind},
    {props::environment::kContrast, &Environment::contrast},
    {props::environment::kSaturation, &Environment::saturation},
    {props::environment::kGrain, &Environment::grain},
    {props::environment::kAberration, &Environment::aberration},
};
// The ones that are a 0..1 strength.
constexpr NumberProp kStrengthProps[] = {
    {props::environment::kVignette, &Environment::vignette},
    {props::environment::kShadows, &Environment::shadows},
    {props::environment::kAmbientOcclusion, &Environment::ambient_occlusion},
};

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

Vec3 rgb(const Color& color) { return {color.r, color.g, color.b}; }

PointLight point_light(const LightDesc& light, Vec3 position) {
  return {position, rgb(light.color) * (light.intensity * config::kPointWattScale),
          light.range > 0.0f ? light.range : config::kDefaultPointLightRange};
}

// How far a light's reach ends from the viewer: a far but wide light can matter more than a near
// but tiny one.
float reach(const PointLight& light, Vec3 viewer) { return length(light.position - viewer) - light.range; }

// Lights fade to nothing as their reach approaches `cut`, over `fade_distance` meters.
void fade_towards(std::vector<PointLight>& lights, Vec3 viewer, float cut, float fade_distance) {
  for (PointLight& light : lights) {
    light.color = light.color * std::clamp((cut - reach(light, viewer)) / fade_distance, 0.0f, 1.0f);
  }
}

// The sky, the ground and the fog of a clear day.
Environment day_sky() {
  Environment env;
  env.sky_top = kDaySkyTop;
  env.sky_horizon = kDaySkyHorizon;
  env.ground = kDayGround;
  env.fog_color = kDaySkyHorizon;
  env.fog_density = kDayFogDensity;
  return env;
}

// A light of the scene: one more point light, or the sun (the first directional light; the
// others are ignored).
void add_light(SceneLights& lights, const LightDesc& light, const Transform& world, bool& sun_found) {
  if (light.type != LightType::Directional) {
    lights.points.push_back(point_light(light, world.position));
    return;
  }
  if (sun_found) return;
  sun_found = true;
  lights.sun_direction = world.forward();
  lights.sun_color = rgb(light.color) * (light.intensity * config::kSunStrengthScale);
}

}  // namespace

Environment default_environment() {
  Environment env = day_sky();
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
  read_color(props, props::environment::kSkyTop, env.sky_top);
  read_color(props, props::environment::kSkyHorizon, env.sky_horizon);
  read_color(props, props::environment::kGroundColor, env.ground);
  env.fog_color = env.sky_horizon;  // fog fades into the horizon unless given its own color
  read_color(props, props::environment::kFogColor, env.fog_color);
  for (const NumberProp& prop : kNumberProps) read_number(props, prop.key, env.*prop.value);
  for (const NumberProp& prop : kStrengthProps) read_strength(props, prop.key, env.*prop.value);
  return env;
}

Environment scene_environment(const Scene& scene) {
  for (const ObjectId id : scene.alive_ids()) {
    const PropertyMap& props = scene.at(id).props;
    if (get_bool(props, props::kEnvironment).value_or(false)) return environment_from_props(props);
  }
  return default_environment();
}

std::vector<PointLight> nearest_lights(std::vector<PointLight> lights, Vec3 viewer, std::size_t max_count,
                                       float fade_distance) {
  std::stable_sort(lights.begin(), lights.end(),
                   [viewer](const PointLight& a, const PointLight& b) { return reach(a, viewer) < reach(b, viewer); });
  if (lights.size() <= max_count) return lights;
  // The first light left out marks the cut: kept lights fade to nothing as they approach it.
  const float cut = reach(lights[max_count], viewer);
  lights.resize(max_count);
  if (fade_distance > 0.0f) fade_towards(lights, viewer, cut, fade_distance);
  return lights;
}

SceneLights scene_lights(const Scene& scene) {
  SceneLights lights = default_lights();
  bool sun_found = false;
  for (const ObjectId id : scene.alive_ids()) {
    const GameObject& object = scene.at(id);
    if (!object.light || object.prefab) continue;
    add_light(lights, *object.light, scene.world_transform(id), sun_found);
  }
  return lights;
}

}  // namespace redgem
