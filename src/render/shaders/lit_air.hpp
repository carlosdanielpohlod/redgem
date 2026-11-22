#pragma once

// The lit fragment shader's air (see lit_fragment.hpp): what hides a surface on the way to the eye.
// A fog that can lie low and thin out going up, lit by the sun the eye looks toward, and a thin
// haze over long distances that takes blue out of the light first.

namespace redgem::shaders {

inline constexpr const char* kLitAir = R"GLSL(
uniform vec3 fogColor;
uniform float fogDensity;     // per meter, at fogHeight
uniform float fogHeight;      // meters: where the fog is fogDensity thick
uniform float fogFalloff;     // per meter up, the fog thins by e this many times (0: as thick everywhere)
uniform float fogSunScatter;  // how much sunlight the fog throws on toward the eye (0: none)
uniform float hazeDensity;    // per meter at height 0: the thin air of long distances (0: none)

const float HAZE_FALLOFF = 1.0 / 1200.0;            // the haze thins by e every 1200 m up
const vec3 HAZE_SPECTRUM = vec3(0.55, 0.75, 1.0);   // blue is scattered most, red least
const float SUN_FORWARD = 0.75;                     // how much fog throws light on rather than around
const float LEVEL_RAY = 0.001;                      // a ray this little thinner or thicker counts as level
const float AIR_EXPONENT_LIMIT = 40.0;              // far below thick fog: as good as opaque already

// How much of a layer of fog the ray from the eye to a point goes through (its optical depth): a
// layer `density` thick at `base`, thinning by e every 1/falloff meters up.
float layerDepth(float density, float falloff, float base, vec3 point) {
  if (density <= 0.0) return 0.0;
  vec3 ray = point - viewPos;
  float distance = length(ray);
  if (falloff <= 0.0) return density * distance;
  float atEye = density * exp(min(-falloff * (viewPos.y - base), AIR_EXPONENT_LIMIT));
  float thinning = min(falloff * ray.y, AIR_EXPONENT_LIMIT);
  float along = abs(thinning) < LEVEL_RAY ? 1.0 - 0.5 * thinning : (1.0 - exp(-thinning)) / thinning;
  return atEye * distance * along;
}

// The share of sunlight fog throws toward the eye looking along `ray` (Henyey-Greenstein): most of
// it looking into the sun.
float sunPhase(vec3 ray) {
  float toward = dot(normalize(ray), normalize(-sunDirection));
  float g2 = SUN_FORWARD * SUN_FORWARD;
  return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * SUN_FORWARD * toward, 1.5));
}

// What the air between the eye and a point lets through of it, and the light it has of its own.
struct Air {
  vec3 through;
  vec3 glow;
};

Air airTo(vec3 point) {
  float fog = layerDepth(fogDensity, fogFalloff, fogHeight, point);
  vec3 haze = HAZE_SPECTRUM * layerDepth(hazeDensity, HAZE_FALLOFF, 0.0, point);
  vec3 sunlit = sunColor * fogSunScatter * sunPhase(point - viewPos);
  return Air(exp(-(vec3(fog) + haze)), fogColor + sunlit);
}

// A surface's color as the eye sees it through the air.
vec3 throughAir(vec3 color, vec3 point) {
  Air air = airTo(point);
  return color * air.through + air.glow * (1.0 - air.through);
}
)GLSL";

}  // namespace redgem::shaders
