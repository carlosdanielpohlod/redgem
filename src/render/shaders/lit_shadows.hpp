#pragma once

// The lit fragment shader's part that knows the sun's shadows (see lit_fragment.hpp). The shadow
// map holds one picture of the scene's depth as the sun sees it, or several (cascades: tiles of
// the map, each covering a larger stretch farther from the camera); a point is looked up in the
// first one it lies well inside. Shadows as soft as the sun is wide are compiled in only when a
// scene asks for them (SOFT_SHADOWS).

namespace redgem::shaders {

inline constexpr const char* kLitShadows = R"GLSL(
const int MAX_CASCADES = 4;              // render_math::kMaxShadowCascades
const float SHADOW_NORMAL_OFFSET = 0.06; // meters the lookup moves off the surface (prevents acne)
const float SHADOW_MIN_BIAS = 0.02;      // meters
const float SHADOW_SLOPE_BIAS = 0.12;    // meters more on surfaces edge-on to the sun
const int SHADOW_KERNEL = 2;             // (2k+1)^2 taps for soft edges
const int SHADOW_TAPS_ACROSS = SHADOW_KERNEL + 1 + SHADOW_KERNEL;  // the texel itself and its neighbours on both sides
const float SHADOW_FADE = 0.08;          // fraction of the map's border where shadows fade out
const float SHADOW_BASE_EXTENT = 90.0;   // meters across the whole map when it is one picture (render_math::kShadowExtent)
const int SOFT_TAPS = 16;
const int SEARCH_TAPS = 8;               // every other tap of the disc, for casters
const int SOFT_CASCADES = 2;             // the nearest cascades, whose edges are worked out by the sun's size
const float BLOCKER_REACH = 12.0;        // meters above a point casters are looked for, for soft shadows
const float MIN_SOFT_TEXELS = 1.5;       // the narrowest soft edge, in texels
const float MIN_FACING = 0.2;            // a surface more edge-on to the sun is taken as this steep
const float MAX_SLOPE = 4.0;
const vec2 SOFT_DISC[SOFT_TAPS] = vec2[SOFT_TAPS](
    vec2(-0.942, -0.399), vec2(0.946, -0.769), vec2(-0.094, -0.929), vec2(0.345, 0.294),
    vec2(-0.916, 0.458), vec2(-0.815, -0.879), vec2(-0.383, 0.277), vec2(0.975, 0.756),
    vec2(0.443, -0.975), vec2(0.537, -0.474), vec2(-0.265, -0.419), vec2(0.792, 0.191),
    vec2(-0.242, 0.997), vec2(-0.814, 0.914), vec2(0.200, 0.786), vec2(0.144, -0.141));

uniform sampler2D shadowMap;       // depth of the scene seen from the sun
uniform mat4 lightViewProjection[MAX_CASCADES];
uniform vec4 shadowTiles[MAX_CASCADES];    // where each cascade is in the map: its corner (xy) and size (zw)
uniform float shadowScales[MAX_CASCADES];  // how much larger a texel of each is than one of the map taken whole
uniform int shadowCascades;
uniform float shadowStrength;      // 0 = no shadows
uniform float shadowTexel;         // size of one shadow map texel in UV
uniform float shadowDepthRange;    // meters between the near and far planes of the sun's view
uniform float sunSize;             // the tangent of the sun's apparent radius (soft shadows)

vec3 shadowCoords(int cascade, vec3 position) {
  vec4 clip = lightViewProjection[cascade] * vec4(position, 1.0);
  return clip.xyz / clip.w * HALF + HALF;
}

float edgeDistance(vec3 coords) {
  vec2 border = min(coords.xy, 1.0 - coords.xy);
  return min(border.x, border.y);
}

// The first cascade a point lies well inside (the last one takes everything else).
int cascadeOf(vec3 position) {
  for (int i = 0; i < shadowCascades - 1; ++i) {
    vec3 coords = shadowCoords(i, position);
    if (edgeDistance(coords) > SHADOW_FADE && coords.z <= 1.0) return i;
  }
  return shadowCascades - 1;
}

// Percentage-closer filtering: the share of the texels around that the point is not behind.
float filteredShadow(vec2 uv, float depth) {
  float lit = 0.0;
  for (int x = -SHADOW_KERNEL; x <= SHADOW_KERNEL; ++x) {
    for (int y = -SHADOW_KERNEL; y <= SHADOW_KERNEL; ++y) {
      float nearest = texture(shadowMap, uv + vec2(x, y) * shadowTexel).r;
      lit += depth > nearest ? 0.0 : 1.0;
    }
  }
  return lit / float(SHADOW_TAPS_ACROSS * SHADOW_TAPS_ACROSS);
}

vec2 discTap(int i, vec2 turn) {
  vec2 p = SOFT_DISC[i];
  return vec2(p.x * turn.x - p.y * turn.y, p.x * turn.y + p.y * turn.x);
}

// How far a surface drops away from the sun per meter across the sun's view (held to a few).
float surfaceSlope(vec3 n, vec3 toSun) {
  float facing = clamp(dot(n, toSun), MIN_FACING, 1.0);
  return min(sqrt(1.0 - facing * facing) / facing, MAX_SLOPE);
}

vec2 inTile(vec2 uv, vec4 tile) { return clamp(uv, tile.xy + shadowTexel, tile.xy + tile.zw - shadowTexel); }

// Shadows as soft as the sun is wide: the farther a caster is above the point, the wider the
// edge. Casters are looked for first, then the edge is filtered as wide as their distance makes it.
// `slope` (how far the surface drops per meter across, seen from the sun) keeps a surface from
// shadowing itself where the filter reaches far across it.
float softShadow(vec2 uv, float depth, int cascade, float slope) {
  vec4 tile = shadowTiles[cascade];
  float metersPerUv = SHADOW_BASE_EXTENT * shadowScales[cascade];
  float angle = pixelNoise() * TWO_PI;
  vec2 turn = vec2(cos(angle), sin(angle));
  float search = sunSize * BLOCKER_REACH / metersPerUv;
  float searchDepth = depth - search * metersPerUv * slope / shadowDepthRange;
  float blockers = 0.0;
  float found = 0.0;
  for (int i = 0; i < SEARCH_TAPS; ++i) {
    float nearest = texture(shadowMap, inTile(uv + discTap(i * 2, turn) * search, tile)).r;
    if (nearest < searchDepth) { blockers += nearest; found += 1.0; }
  }
  if (found <= 0.0) return 1.0;
  if (found >= float(SEARCH_TAPS)) return 0.0;  // well inside the shadow: no edge to filter
  float radius = max(sunSize * (depth - blockers / found) * shadowDepthRange / metersPerUv, shadowTexel * MIN_SOFT_TEXELS);
  float filterDepth = depth - radius * metersPerUv * slope / shadowDepthRange;
  float lit = 0.0;
  for (int i = 0; i < SOFT_TAPS; ++i) {
    lit += filterDepth > texture(shadowMap, inTile(uv + discTap(i, turn) * radius, tile)).r ? 0.0 : 1.0;
  }
  return lit / float(SOFT_TAPS);
}

// How much of the sun reaches a point: 1 = fully lit, 0 = in shadow. Outside the shadow map
// everything is lit.
float sunVisibility(vec3 position, vec3 n, vec3 toSun) {
  if (shadowStrength <= 0.0) return 1.0;
  int cascade = cascadeOf(position);
  float scale = shadowScales[cascade];
  vec3 coords = shadowCoords(cascade, position + n * SHADOW_NORMAL_OFFSET * scale);
  if (coords.z > 1.0) return 1.0;
  float inside = smoothstep(0.0, SHADOW_FADE, edgeDistance(coords));
  if (inside <= 0.0) return 1.0;
  float bias = max(SHADOW_SLOPE_BIAS * (1.0 - max(dot(n, toSun), 0.0)), SHADOW_MIN_BIAS) * scale / shadowDepthRange;
  vec4 tile = shadowTiles[cascade];
  vec2 uv = tile.xy + coords.xy * tile.zw;
#ifdef SOFT_SHADOWS
  // (Far cascades' texels are wider than the softest edge they would show: filtered as usual.)
  float lit = cascade < SOFT_CASCADES ? softShadow(uv, coords.z - bias, cascade, surfaceSlope(n, toSun))
                                      : filteredShadow(uv, coords.z - bias);
#else
  float lit = filteredShadow(uv, coords.z - bias);
#endif
  return mix(1.0, lit, shadowStrength * inside);
}
)GLSL";

}  // namespace redgem::shaders
