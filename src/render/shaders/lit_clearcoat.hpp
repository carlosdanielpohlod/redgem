#pragma once

// A dielectric coating above the base material, using KHR_materials_clearcoat's Fresnel layering.
// Its normal and roughness are independent: polished varnish can cover a rough, textured base.

namespace redgem::shaders {

inline constexpr const char* kLitClearcoat = R"GLSL(
#ifdef CLEARCOAT
uniform float coatFactor;
uniform float coatRoughness;
uniform sampler2D coatFactorMap;       // linear R: amount of coating
uniform sampler2D coatRoughnessMap;    // linear G: coating roughness
uniform sampler2D coatNormalMap;
uniform float hasCoatFactorMap;
uniform float hasCoatRoughnessMap;
uniform float hasCoatNormalMap;
uniform float coatNormalScale;

float coatWeight;
float coatRough;
vec3 coatNormal;

void prepareCoat(vec3 geometricNormal) {
  coatWeight = clamp(coatFactor, 0.0, 1.0);
  coatNormal = geometricNormal;
  coatRough = coatRoughness;
  if (coatWeight <= 0.0) return;
  if (hasCoatFactorMap > SWITCH_ON) coatWeight *= texture(coatFactorMap, fragTexCoord).r;
  if (hasCoatRoughnessMap > SWITCH_ON) coatRough *= texture(coatRoughnessMap, fragTexCoord).g;
  coatRough = clamp(coatRough, MIN_ROUGHNESS, 1.0);
  if (hasCoatNormalMap > SWITCH_ON) {
    coatNormal = mappedNormal(geometricNormal, fragPosition, fragTexCoord, coatNormalMap, coatNormalScale);
  }
}

float coatBlend(vec3 v) { return coatWeight * fresnel(abs(dot(coatNormal, v)), vec3(DIELECTRIC_F0)).r; }

vec3 shadeSurface(vec3 n, vec3 v, vec3 l, vec3 radiance, vec3 albedo, vec3 f0, float rough, float metal) {
  vec3 base = shade(n, v, l, radiance, albedo, f0, rough, metal);
  if (coatWeight <= 0.0) return base;
  // Unit Fresnel and no diffuse give the coat's bare microfacet lobe; coatBlend weights it once.
  vec3 layer = shade(coatNormal, v, l, radiance, vec3(0.0), vec3(1.0), coatRough, 1.0);
  return mix(base, layer, coatBlend(v));
}

vec3 ambientSurface(vec3 n, vec3 v, vec3 albedo, vec3 f0, float rough, float metal) {
  vec3 base = ambientLight(n, v, albedo, f0, rough, metal);
  if (coatWeight <= 0.0) return base;
  vec3 layer = ambientLight(coatNormal, v, vec3(0.0), vec3(1.0), coatRough, 1.0);
  return mix(base, layer, coatBlend(v));
}
#else
#define shadeSurface shade
#define ambientSurface ambientLight
#endif
)GLSL";

}  // namespace redgem::shaders
