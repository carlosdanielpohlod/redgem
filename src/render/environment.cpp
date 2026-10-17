#include "render/environment.hpp"

#include <algorithm>

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
    {props::environment::kGlowLimit, &Environment::glow_limit},
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
  read_sky_and_quality(props, env);
  return env;
}

Environment scene_environment(const Scene& scene) {
  for (const ObjectId id : scene.alive_ids()) {
    const PropertyMap& props = scene.at(id).props;
    if (get_bool(props, props::kEnvironment).value_or(false)) return environment_from_props(props);
  }
  return default_environment();
}

}  // namespace redgem
