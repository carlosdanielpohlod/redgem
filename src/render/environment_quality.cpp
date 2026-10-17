// An environment's sky picture and what it asks of the renderer's quality (see environment.hpp).

#include <algorithm>
#include <cmath>

#include "render/environment.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr int kFewestSamples = 2;            // fewer is no multisampling at all
constexpr int kMostSamples = 8;              // what every desktop card holds (phones: 4, which they all hold)
constexpr int kDoubling = 2;                 // samples come in powers of two
constexpr int kMostCascades = 4;             // render_math::kMaxShadowCascades
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

}  // namespace redgem
