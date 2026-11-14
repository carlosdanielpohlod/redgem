#pragma once

// The lit fragment shader's leaves lit from behind (see lit_fragment.hpp): sunlight that comes
// through a thin leaf toward the eye, after the sun's shadows (lit_shadows.hpp), which it is held
// back by on the side facing the sun.

namespace redgem::shaders {

inline constexpr const char* kLitFoliage = R"GLSL(
uniform float foliageTranslucency;  // 0 to 1 of the sunlight a leaf lets through (0: not foliage)

// Of the light through a leaf, the share that glows most looking into the sun: the rest spreads
// out as a soft light all over the leaf's back.
const float LEAF_GLARE_SHARE = 0.5;
const float LEAF_GLARE_POWER = 4.0;  // how tight that glow is around the sun

// The sunlight that comes through a leaf whose lit side faces away from the eye: `n` is the side
// the eye sees. Tinted by the leaf's own color, as light through a leaf is.
vec3 throughLeaf(vec3 n, vec3 v, vec3 toSun, vec3 albedo) {
  float behind = dot(-n, toSun);
  if (foliageTranslucency <= 0.0 || behind <= 0.0) return vec3(0.0);
  float glare = pow(clamp(dot(-v, toSun), 0.0, 1.0), LEAF_GLARE_POWER);
  float through = mix(behind, glare, LEAF_GLARE_SHARE);
  return albedo * sunColor * foliageTranslucency * through * sunVisibility(fragPosition, -n, toSun);
}
)GLSL";

}  // namespace redgem::shaders
