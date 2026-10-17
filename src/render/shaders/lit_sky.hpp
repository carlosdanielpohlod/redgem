#pragma once

// The lit fragment shader's part that knows the light from all around (see lit_fragment.hpp): the
// sky, as a gradient or as a picture (its light worked out when it was loaded: render/sky_light.hpp),
// mixed with the surroundings the engine keeps capturing around the camera. A picture's light is
// compiled in only when there is one (SKY_PICTURE): scenes without one do not pay for it.

namespace redgem::shaders {

inline constexpr const char* kLitSky = R"GLSL(
const float HORIZON_BLEND = 0.25;        // softness of the ground/sky seam in the ambient
const float ZENITH_CURVE = 0.6;          // how quickly the horizon color turns into the zenith color
const float ROUGH_SKY_BLUR = 0.7;
const float ROUGH_SPECULAR_DAMPING = 0.5;
const float REFLECTION_MAX_LOD = 6.0;    // blurriest level used, for fully rough surfaces
const float REFLECTION_DIFFUSE_LOD = 8.0;
const float REFLECTION_DIFFUSE_MIX = 0.5; // how much the surroundings (not just the sky) color the ambient light
const float MAX_REFLECTION = 12.0;       // the sun's disc must not turn glossy surfaces into mirrors of it
const int SKY_HARMONICS = 9;             // render_math::kSkyHarmonics
const float SKY_ROUGHEST_LEVEL = 5.0;    // render_math::kSkyRoughestLevel
// The real spherical harmonics' constants (render/sky_harmonics.cpp).
const float BAND0 = 0.282095;
const float BAND1 = 0.488603;
const float BAND2_CROSS = 1.092548;
const float BAND2_ZONAL = 0.315392;
const float BAND2_SQUARE = 0.546274;
const float THREE = 3.0;

uniform float ambientStrength;
uniform vec3 skyTop;
uniform vec3 skyHorizon;
uniform vec3 groundColor;
uniform sampler2D reflectionMap;   // the surroundings as a panorama (longitude x latitude), HDR, with mips
uniform float reflectionStrength;  // 0 until the panorama has been captured
uniform vec3 skyIrradiance[SKY_HARMONICS];
uniform samplerCube skyReflections; // the picture blurred for each roughness, a mip level each
uniform sampler2D skyReflectance;   // how much of that a surface reflects (split sum)
uniform vec2 skyTurn;               // cosine and sine of the turn that leads from the world into the picture
uniform float skyIntensity;

// Where a direction falls on the panorama. Must match the capture (kPanoramaFragment).
vec2 panoramaUv(vec3 d) {
  return vec2(atan(d.x, -d.z) / TWO_PI + HALF, asin(clamp(d.y, -1.0, 1.0)) / PI + HALF);
}

vec3 surroundings(vec3 direction, float lod) {
  return min(textureLod(reflectionMap, panoramaUv(direction), lod).rgb, vec3(MAX_REFLECTION));
}

// The sky as surfaces see it: ground color below the horizon, horizon color turning into the
// zenith color above. `blur` widens the seam for rough surfaces.
vec3 sky(vec3 direction, float blur) {
  float above = smoothstep(-HORIZON_BLEND - blur, HORIZON_BLEND + blur, direction.y);
  vec3 upper = mix(skyHorizon, skyTop, pow(clamp(direction.y, 0.0, 1.0), ZENITH_CURVE));
  return mix(groundColor, upper, above);
}

vec3 intoPicture(vec3 d) { return vec3(d.x * skyTurn.x - d.z * skyTurn.y, d.y, d.x * skyTurn.y + d.z * skyTurn.x); }

// What a matte surface facing `n` receives from the picture (render_math::irradiance_toward).
vec3 pictureIrradiance(vec3 n) {
  vec3 d = intoPicture(n);
  float basis[SKY_HARMONICS] = float[SKY_HARMONICS](BAND0, BAND1 * d.y, BAND1 * d.z, BAND1 * d.x, BAND2_CROSS * d.x * d.y,
      BAND2_CROSS * d.y * d.z, BAND2_ZONAL * (THREE * d.z * d.z - 1.0), BAND2_CROSS * d.x * d.z, BAND2_SQUARE * (d.x * d.x - d.y * d.y));
  vec3 total = vec3(0.0);
  for (int i = 0; i < SKY_HARMONICS; ++i) total += skyIrradiance[i] * basis[i];
  return max(total, vec3(0.0)) * skyIntensity;
}

vec3 pictureReflection(vec3 mirror, float rough) {
  return textureLod(skyReflections, intoPicture(mirror), rough * SKY_ROUGHEST_LEVEL).rgb * skyIntensity;
}

// What a surface reflects of the light around it, besides the sun and the lamps.
vec3 ambientLight(vec3 n, vec3 v, vec3 albedo, vec3 f0, float rough, float metal) {
  vec3 f = fresnel(max(dot(n, v), 0.0), f0);
  vec3 mirror = reflect(-v, n);
#ifdef SKY_PICTURE
  vec3 fromAround = pictureIrradiance(n);
  vec3 reflected = pictureReflection(mirror, rough);
#else
  vec3 fromAround = sky(n, ROUGH_SKY_BLUR);
  vec3 reflected = sky(mirror, rough * ROUGH_SKY_BLUR);
#endif
  if (reflectionStrength > 0.0) {
    fromAround = mix(fromAround, surroundings(n, REFLECTION_DIFFUSE_LOD), REFLECTION_DIFFUSE_MIX * reflectionStrength);
    reflected = mix(reflected, surroundings(mirror, rough * REFLECTION_MAX_LOD), reflectionStrength);
  }
  vec3 diffuseAmbient = (1.0 - f) * (1.0 - metal) * albedo * fromAround;
  // With a picture, the share reflected is the split sum's table; without, an estimate of it.
#ifdef SKY_PICTURE
  vec2 table = texture(skyReflectance, vec2(max(dot(n, v), 0.0), rough)).rg;
  return diffuseAmbient + reflected * (f0 * table.x + table.y);
#else
  return diffuseAmbient + f * reflected * (1.0 - rough * ROUGH_SPECULAR_DAMPING);
#endif
}
)GLSL";

}  // namespace redgem::shaders
