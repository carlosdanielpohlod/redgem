// An environment's sky picture and what it asks of the renderer's quality (see environment.hpp).

#include <algorithm>
#include <cmath>

#include "render/environment.hpp"
#include "render/render_config.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr int kFewestSamples = 2;  // fewer is no multisampling at all
constexpr int kMostSamples = config::kMostSamples;
constexpr int kDoubling = 2;  // samples come in powers of two
constexpr int kMostCascades = config::kMostCascades;
constexpr float kCascadesReach = 150.0f;     // meters, unless the environment says
constexpr float kFarthestShadows = 2000.0f;  // meters
// Meters: the nearest shadows reach. More than any camera's near plane, so that a cascade has room.
constexpr float kNearestShadows = 1.0f;
constexpr float kWidestSun = 10.0f;  // degrees: wider makes every shadow a blur

float number_or(const PropertyMap& props, const char* key, float fallback) {
  const auto value = get_number(props, key);
  return value && std::isfinite(*value) ? static_cast<float>(*value) : fallback;
}

// The largest power of two up to `asked` (and kMostSamples), or 0 below kFewestSamples.
int samples_of(float asked) {
  if (!(asked >= static_cast<float>(kFewestSamples))) return 0;
  // (Held to the range before it becomes an int: a float past an int's range does not fit one.)
  const int most = static_cast<int>(std::min(asked, static_cast<float>(kMostSamples)));
  int samples = kFewestSamples;
  while (samples * kDoubling <= most) samples *= kDoubling;
  return samples;
}

int cascades_of(float asked) {
  return static_cast<int>(std::lround(std::clamp(asked, 1.0f, static_cast<float>(kMostCascades))));
}

// The share a quality allows of what a level asks for: within 0 (none of it) and 1 (all of it).
float allowed_share(float share) { return std::isfinite(share) ? std::clamp(share, 0.0f, 1.0f) : 1.0f; }

// Smoothed edges: as many samples as the level and the quality allow, and none when the card cannot.
int allowed_samples(int asked, int allowed, const GpuAbilities& gpu) {
  if (!gpu.multisampling) return 0;
  return samples_of(static_cast<float>(std::min(asked, allowed)));
}

bool allowed_temporal(bool asked, const QualityCaps& caps, const GpuAbilities& gpu) {
  return asked && caps.temporal_antialias && gpu.temporal;
}

}  // namespace

void read_sky_and_quality(const PropertyMap& props, Environment& env) {
  env.sky_image = get_string(props, props::environment::kSkyImage).value_or("");
  env.sky_rotation = degrees_to_radians(number_or(props, props::environment::kSkyRotation, 0.0f));
  env.sky_intensity = std::max(number_or(props, props::environment::kSkyIntensity, env.sky_intensity), 0.0f);
  env.msaa = samples_of(number_or(props, props::environment::kMsaa, 0.0f));
  env.shadow_cascades = cascades_of(number_or(props, props::environment::kShadowCascades, 1.0f));
  env.shadow_distance = std::clamp(number_or(props, props::environment::kShadowDistance, kCascadesReach),
                                   kNearestShadows, kFarthestShadows);
  const float softness = std::clamp(number_or(props, props::environment::kShadowSoftness, 0.0f), 0.0f, kWidestSun);
  env.shadow_softness = degrees_to_radians(softness);
  env.temporal_antialias = get_string(props, props::environment::kAntialias) == props::environment::kTemporalAntialias;
}

Environment within_quality(Environment env, const QualityCaps& caps, const GpuAbilities& gpu) {
  env.msaa = allowed_samples(env.msaa, caps.msaa, gpu);
  env.temporal_antialias = allowed_temporal(env.temporal_antialias, caps, gpu);
  env.shadow_cascades = std::clamp(caps.shadow_cascades, 1, env.shadow_cascades);
  // (Never nearer than the nearest shadows reach: a cascade needs room.)
  const float nearest = std::min(env.shadow_distance, kNearestShadows);
  env.shadow_distance = std::max(env.shadow_distance * allowed_share(caps.shadow_distance), nearest);
  env.ambient_occlusion *= allowed_share(caps.ambient_occlusion);
  env.bloom *= allowed_share(caps.bloom);
  return env;
}

}  // namespace redgem
