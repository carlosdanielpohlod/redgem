#pragma once

// The lit fragment shader's leaves lit from behind (see lit_fragment.hpp): sunlight that comes
// through a thin leaf toward the eye, held back by the sun's shadow (lit_shadows.hpp) on the
// leaf's side facing the sun.

namespace redgem::shaders {

inline constexpr const char* kLitFoliage = R"GLSL(
uniform float foliageTranslucency;  // 0 to 1 of the sunlight a leaf lets through (0: not foliage)

// Of the light through a leaf, the share that glows most looking into the sun: the rest spreads
// out as a soft light all over the leaf's back.
const float LEAF_GLARE_SHARE = 0.5;
const float LEAF_GLARE_POWER = 4.0;  // how tight that glow is around the sun

// Whether the sun lights the side of a leaf away from the eye (`n` is the side the eye sees), and
// the leaf lets that light through.
bool litThrough(vec3 n, vec3 toSun) { return foliageTranslucency > 0.0 && dot(n, toSun) < 0.0; }

// The side of a surface its sun shadow is looked up on: a leaf lit through, on its far side (the
// one the sun reaches), so one lookup serves both its sides.
vec3 sunSide(vec3 n, vec3 toSun) { return litThrough(n, toSun) ? -n : n; }

// The sunlight that comes through a leaf lit from behind, before its shadow: tinted by the leaf's
// own color, as light through a leaf is.
vec3 throughLeaf(vec3 n, vec3 v, vec3 toSun, vec3 albedo) {
  if (!litThrough(n, toSun)) return vec3(0.0);
  float glare = pow(clamp(dot(-v, toSun), 0.0, 1.0), LEAF_GLARE_POWER);
  float through = mix(dot(-n, toSun), glare, LEAF_GLARE_SHARE);
  return albedo * sunColor * foliageTranslucency * through;
}
)GLSL";

}  // namespace redgem::shaders
