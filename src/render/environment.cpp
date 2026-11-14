#include "render/environment.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

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
constexpr float kDayFoliageTranslucency = 0.5f;
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
    {props::environment::kFoliageTranslucency, &Environment::foliage_translucency},
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

// A finite number a scene gives, as a float.
std::optional<float> finite_number(const PropertyMap& props, const char* key) {
  const auto value = get_number(props, key);
  if (!value || !std::isfinite(*value)) return std::nullopt;
  return static_cast<float>(*value);
}

// Auto exposure's numbers, the setting each one is, and the range it is held to: EV within
// kWidestExposureEv either way, speeds per second.
struct ExposureProp {
  const char* key;
  float AutoExposure::* value;
  float low;
  float high;
};
constexpr ExposureProp kExposureProps[] = {
    {props::environment::kExposureMinEv, &AutoExposure::min_ev, -kWidestExposureEv, kWidestExposureEv},
    {props::environment::kExposureMaxEv, &AutoExposure::max_ev, -kWidestExposureEv, kWidestExposureEv},
    {props::environment::kExposureCompensation, &AutoExposure::compensation, -kWidestExposureEv, kWidestExposureEv},
    {props::environment::kExposureSpeedUp, &AutoExposure::speed_up, kNoExposureSpeed, kFastestExposureSpeed},
    {props::environment::kExposureSpeedDown, &AutoExposure::speed_down, kNoExposureSpeed, kFastestExposureSpeed},
};

void read_exposure_number(const PropertyMap& props, const ExposureProp& prop, AutoExposure& settings) {
  if (const auto number = finite_number(props, prop.key))
    settings.*prop.value = std::clamp(*number, prop.low, prop.high);
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
  env.foliage_translucency = kDayFoliageTranslucency;
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
  read_auto_exposure(props, env.auto_exposure);
  return env;
}

void read_auto_exposure(const PropertyMap& props, AutoExposure& settings) {
  settings.enabled = get_bool(props, props::environment::kAutoExposure).value_or(settings.enabled);
  for (const ExposureProp& prop : kExposureProps) read_exposure_number(props, prop, settings);
  settings.max_ev = std::max(settings.max_ev, settings.min_ev);
}

void write_auto_exposure(const AutoExposure& settings, PropertyMap& props) {
  props[props::environment::kAutoExposure] = settings.enabled;
  for (const ExposureProp& prop : kExposureProps) props[prop.key] = static_cast<double>(settings.*prop.value);
}

Environment scene_environment(const Scene& scene) {
  for (const ObjectId id : scene.alive_ids()) {
    const PropertyMap& props = scene.at(id).props;
    if (get_bool(props, props::kEnvironment).value_or(false)) return environment_from_props(props);
  }
  return default_environment();
}

}  // namespace redgem
