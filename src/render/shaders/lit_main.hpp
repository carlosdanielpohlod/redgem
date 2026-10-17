#pragma once

// The lit fragment shader's main (see lit_fragment.hpp): a surface's material, then each light on
// it, then the fog.

namespace redgem::shaders {

inline constexpr const char* kLitMain = R"GLSL(
const float MIN_OCCLUSION = 0.0;

void main() {
  if (fadedAway(fragCopy)) discard;
  vec4 texel = texture(texture0, fragTexCoord);
  float alpha = colDiffuse.a * texel.a * fragColor.a;
  bool covering = alphaToCoverage > SWITCH_ON;
  if (covering) alpha = coverage(alpha);
  if (covering ? alpha < EPSILON : leftOut(alpha)) discard;
  vec3 albedo = pow(texel.rgb, vec3(GAMMA)) * colDiffuse.rgb * fragColor.rgb * fragTint;
  float rough = roughness;
  float metal = metallic;
  if (hasSurfaceMap > SWITCH_ON) {
    vec3 surface = texture(surfaceMap, fragTexCoord).rgb;
    rough *= surface.g;
    metal *= surface.b;
  }
  rough = clamp(rough, MIN_ROUGHNESS, 1.0);
  metal = clamp(metal, 0.0, 1.0);
  vec3 n = normalize(fragNormal);
  if (!gl_FrontFacing) n = -n;  // double-sided surfaces (leaves) are lit on both sides
  if (fragCopy.z > 0.0) n = normalize(mix(n, fragUp, fragCopy.z));  // grass lit as the meadow it stands in
  vec3 flatNormal = n;
  if (hasNormalMap > SWITCH_ON) n = mappedNormal(n, fragPosition, fragTexCoord);
  vec3 v = normalize(viewPos - fragPosition);
  vec3 f0 = mix(vec3(DIELECTRIC_F0), albedo, metal);

  vec3 toSun = normalize(-sunDirection);
  vec3 color = shade(n, v, toSun, sunColor * PI, albedo, f0, rough, metal) * sunVisibility(fragPosition, flatNormal, toSun);
  for (int i = 0; i < pointLightCount; ++i) {
    vec3 toLight = pointLightPositions[i] - fragPosition;
    float distance = length(toLight);
    // Inverse-square falloff that reaches exactly zero at the light's range.
    float window = clamp(1.0 - distance / pointLightRanges[i], 0.0, 1.0);
    float falloff = window * window / (distance * distance + LIGHT_FALLOFF_BIAS);
    color += shade(n, v, toLight / distance, pointLightColors[i] * falloff * PI, albedo, f0, rough, metal);
  }

  float occlusion = mix(1.0, max(texture(occlusionMap, fragTexCoord).r, MIN_OCCLUSION), occlusionStrength);
  color += ambientLight(n, v, albedo, f0, rough, metal) * ambientStrength * occlusion;

  vec3 glow = emissive;
  if (hasEmissiveMap > SWITCH_ON) glow *= pow(texture(emissiveMap, fragTexCoord).rgb, vec3(GAMMA));
  color += glow;

  float fog = 1.0 - exp(-fogDensity * length(viewPos - fragPosition));
  color = mix(color, fogColor, clamp(fog, 0.0, 1.0));
  bool shown = covering || alphaMixed > SWITCH_ON;  // its alpha says how much of the pixel it covers
  finalColor = vec4(color, shown ? min(alpha, 1.0) : 1.0);
}
)GLSL";

}  // namespace redgem::shaders
