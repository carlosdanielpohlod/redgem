#pragma once

// Ground layers blended over a terrain (see scene/ground/terrain_layers.hpp), a part of the lit
// fragment shader compiled in only when a scene has a terrain with layers (#define TERRAIN).
//
// Each spot of the ground takes the layers its splat map says, mixed:
// - by height where a layer says so (its sharpness): the higher texel of the layers' height
//   pictures wins near an edge, so sand fills the cracks between rocks instead of fading over them;
// - each layer repeated every `tiling` meters, with large, slow patches of variation, and, when
//   asked, its repeats broken up: the picture is sampled at three random shifts over a grid of
//   hexagons and mixed there (stochastic), so no repeat shows;
// - projected along the three axes where asked (triplanar), so steep slopes are not stretched.
// Pictures are sampled with gradients worked out once for the pixel: the layers skipped where
// their weight is nothing never blur the others' smaller copies.

#include <string>

#include "scene/ground/terrain_layers.hpp"

namespace redgem::shaders {

inline constexpr const char* kTerrainLayers = "terrainLayers";
inline constexpr const char* kTerrainArea = "terrainArea";
inline constexpr const char* kTerrainTiling = "terrainTiling";
inline constexpr const char* kTerrainSharpness = "terrainSharpness";
inline constexpr const char* kTerrainVariation = "terrainVariation";
inline constexpr const char* kTerrainStochastic = "terrainStochastic";
inline constexpr const char* kTerrainTriplanar = "terrainTriplanar";
inline constexpr const char* kTerrainVariationScale = "terrainVariationScale";
inline constexpr const char* kTerrainColors = "terrainColors";
inline constexpr const char* kTerrainSurfaces = "terrainSurfaces";
inline constexpr const char* kTerrainSplat = "terrainSplat";

inline constexpr const char* kLitTerrain = R"GLSL(
#ifdef TERRAIN
in vec3 fragLocal;                       // where the pixel is on its mesh, before it is placed
uniform sampler2DArray terrainColors;    // per layer: color (sRGB), height in alpha
uniform sampler2DArray terrainSurfaces;  // per layer: normal X and Y, roughness, occlusion
uniform sampler2DArray terrainSplat;     // the weights: layers 0-3, then 4-7
uniform int terrainLayers;               // how many (0: the surface is no terrain)
uniform vec4 terrainArea;                // the mesh's smallest X and Z, and 1 over its size along each
uniform float terrainTiling[MAX_TERRAIN_LAYERS];     // repeats per meter
uniform float terrainSharpness[MAX_TERRAIN_LAYERS];  // 0: by weight, 1: by height
uniform float terrainVariation[MAX_TERRAIN_LAYERS];
uniform float terrainStochastic[MAX_TERRAIN_LAYERS];
uniform float terrainTriplanar[MAX_TERRAIN_LAYERS];
uniform float terrainVariationScale;     // patches per meter

const float GROUND_WEIGHT_FLOOR = 0.004;     // a layer with less than about a byte of weight is skipped
const float GROUND_HEIGHT_DEPTH = 0.08;      // how far below the highest layer another still shows
const float GROUND_TRIPLANAR_POWER = 4.0;    // how quickly projections hand over between axes
const float GROUND_HEX_SCALE = 3.4641016;    // 2 sqrt(3): hexagons about a repeat across
const float GROUND_HEX_CONTRAST = 6.0;       // how sharply the three shifted samples hand over
const float GROUND_VARIATION_DARKEST = 0.6;  // a full-strength patch, from this much of the color ...
const float GROUND_VARIATION_HUE = 0.16;     // ... and each channel shifted by up to half this
const int GROUND_FIRST_PICTURE_LAYERS = 4;   // the splat's first picture holds the first four
const vec3 GROUND_LIGHT_OCTAVES = vec3(0.65, 0.35, 2.7);  // the patches' two octaves: shares, and the second's scale
const float GROUND_LAYER_OFFSET = 17.0;      // meters each layer's patches are moved by, so layers differ
const vec3 GROUND_HUE_OFFSETS = vec3(31.0, 57.0, 83.0);
const vec2 GROUND_HEX_SKEW = vec2(-0.57735027, 1.15470054);  // a triangle grid's skew: -1/sqrt(3), 2/sqrt(3)
const vec2 GROUND_HASH_A = vec2(127.1, 311.7);
const vec2 GROUND_HASH_B = vec2(269.5, 183.3);
const float GROUND_HASH_SCALE = 43758.5453;

struct GroundSample {
  vec4 color;    // linear color, height
  vec3 normal;   // in the world
  vec2 surface;  // roughness, occlusion
};

vec2 groundHash(vec2 p) { return fract(sin(vec2(dot(p, GROUND_HASH_A), dot(p, GROUND_HASH_B))) * GROUND_HASH_SCALE); }

float groundNoise(vec2 p) {
  vec2 cell = floor(p);
  vec2 f = fract(p);
  vec2 s = f * f * (3.0 - 2.0 * f);
  float a = groundHash(cell).x;
  float b = groundHash(cell + vec2(1.0, 0.0)).x;
  float c = groundHash(cell + vec2(0.0, 1.0)).x;
  float d = groundHash(cell + vec2(1.0, 1.0)).x;
  return mix(mix(a, b, s.x), mix(c, d, s.x), s.y);
}

// A picture of a layer at `uv`, with its gradients: both arrays at once.
void groundTexels(int layer, vec2 uv, vec2 dx, vec2 dy, out vec4 color, out vec4 surface) {
  color = textureGrad(terrainColors, vec3(uv, float(layer)), dx, dy);
  surface = textureGrad(terrainSurfaces, vec3(uv, float(layer)), dx, dy);
}

// The three corners of the hexagon grid's triangle `uv` is in, and how near it is to each.
void groundHexCorners(vec2 uv, out vec3 weights, out vec2 c1, out vec2 c2, out vec2 c3) {
  vec2 skewed = mat2(1.0, 0.0, GROUND_HEX_SKEW.x, GROUND_HEX_SKEW.y) * (uv * GROUND_HEX_SCALE);
  vec2 base = floor(skewed);
  vec3 t = vec3(fract(skewed), 0.0);
  t.z = 1.0 - t.x - t.y;
  float s = step(0.0, -t.z);
  float flip = 2.0 * s - 1.0;
  weights = vec3(-t.z * flip, s - t.y * flip, s - t.x * flip);
  c1 = base + vec2(s, s);
  c2 = base + vec2(s, 1.0 - s);
  c3 = base + vec2(1.0 - s, s);
}

// A layer's pictures with its repeats broken up: three randomly shifted samples, mixed by how near
// the pixel is to each of its hexagon's corners, sharpened so that one sample wins most of the way.
void groundStochastic(int layer, vec2 uv, vec2 dx, vec2 dy, out vec4 color, out vec4 surface) {
  vec3 near;
  vec2 c1, c2, c3;
  groundHexCorners(uv, near, c1, c2, c3);
  vec4 color1, color2, color3, surface1, surface2, surface3;
  groundTexels(layer, uv + groundHash(c1), dx, dy, color1, surface1);
  groundTexels(layer, uv + groundHash(c2), dx, dy, color2, surface2);
  groundTexels(layer, uv + groundHash(c3), dx, dy, color3, surface3);
  vec3 mixWeights = pow(near, vec3(GROUND_HEX_CONTRAST));
  mixWeights /= max(dot(mixWeights, vec3(1.0)), EPSILON);
  color = color1 * mixWeights.x + color2 * mixWeights.y + color3 * mixWeights.z;
  surface = surface1 * mixWeights.x + surface2 * mixWeights.y + surface3 * mixWeights.z;
}

void groundProjected(int layer, vec2 uv, vec2 dx, vec2 dy, out vec4 color, out vec4 surface) {
  if (terrainStochastic[layer] > SWITCH_ON) {
    groundStochastic(layer, uv, dx, dy, color, surface);
  } else {
    groundTexels(layer, uv, dx, dy, color, surface);
  }
}

// A normal map's X and Y bent onto the world normal `n` (whiteout blending), for pictures laid
// along an axis: `across` and `along` are the world directions of the picture's U and V.
vec3 groundBent(vec3 n, vec4 surface, vec3 across, vec3 along) {
  vec2 tilt = surface.xy * NORMAL_MAP_RANGE - 1.0;
  return n + across * tilt.x + along * tilt.y;
}

// A layer seen from above alone, its normal in the world.
GroundSample groundFromAbove(int layer, vec3 n, vec3 dpx, vec3 dpy) {
  float tiling = terrainTiling[layer];
  vec4 color, surface;
  groundProjected(layer, fragPosition.xz * tiling, dpx.xz * tiling, dpy.xz * tiling, color, surface);
  return GroundSample(color, groundBent(n, surface, vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0)), surface.zw);
}

// A layer projected along each axis, mixed by how much the ground faces it.
GroundSample groundTriplanar(int layer, vec3 n, vec3 dpx, vec3 dpy) {
  float tiling = terrainTiling[layer];
  vec3 facing = pow(abs(n), vec3(GROUND_TRIPLANAR_POWER));
  facing /= max(dot(facing, vec3(1.0)), EPSILON);
  vec4 cx, sx, cy, sy, cz, sz;
  groundProjected(layer, fragPosition.zy * tiling, dpx.zy * tiling, dpy.zy * tiling, cx, sx);
  groundProjected(layer, fragPosition.xz * tiling, dpx.xz * tiling, dpy.xz * tiling, cy, sy);
  groundProjected(layer, fragPosition.xy * tiling, dpx.xy * tiling, dpy.xy * tiling, cz, sz);
  vec3 bent = groundBent(n, sx, vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0)) * facing.x +
              groundBent(n, sy, vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0)) * facing.y +
              groundBent(n, sz, vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0)) * facing.z;
  vec4 surface = sx * facing.x + sy * facing.y + sz * facing.z;
  return GroundSample(cx * facing.x + cy * facing.y + cz * facing.z, bent, surface.zw);
}

// Large patches of a layer darker or lighter (60% to 140% of its color) and shifted in hue, by
// how much it varies.
vec3 groundVaried(int layer, vec3 color) {
  float amount = terrainVariation[layer];
  if (amount <= 0.0) return color;
  vec2 at = fragPosition.xz * terrainVariationScale + vec2(float(layer) * GROUND_LAYER_OFFSET);
  float light = groundNoise(at) * GROUND_LIGHT_OCTAVES.x + groundNoise(at * GROUND_LIGHT_OCTAVES.z) * GROUND_LIGHT_OCTAVES.y;
  vec3 hue = vec3(groundNoise(at + GROUND_HUE_OFFSETS.x), groundNoise(at + GROUND_HUE_OFFSETS.y),
                  groundNoise(at + GROUND_HUE_OFFSETS.z)) - HALF;
  vec3 varied = color * mix(GROUND_VARIATION_DARKEST, 1.0 + (1.0 - GROUND_VARIATION_DARKEST), light);
  return mix(color, varied * (1.0 + hue * GROUND_VARIATION_HUE), amount);
}

GroundSample groundLayer(int layer, vec3 n, vec3 dpx, vec3 dpy) {
  GroundSample ground = terrainTriplanar[layer] > SWITCH_ON ? groundTriplanar(layer, n, dpx, dpy)
                                                            : groundFromAbove(layer, n, dpx, dpy);
  ground.color.rgb = groundVaried(layer, pow(ground.color.rgb, vec3(GAMMA)));
  return ground;
}

// What the splat map says of each layer here.
void groundWeights(out float weights[MAX_TERRAIN_LAYERS]) {
  vec2 uv = (fragLocal.xz - terrainArea.xy) * terrainArea.zw;
  vec4 first = texture(terrainSplat, vec3(uv, 0.0));
  vec4 second = terrainLayers > GROUND_FIRST_PICTURE_LAYERS ? texture(terrainSplat, vec3(uv, 1.0)) : vec4(0.0);
  float drawn = 0.0;
  for (int i = 0; i < MAX_TERRAIN_LAYERS; ++i) {
    weights[i] = i < GROUND_FIRST_PICTURE_LAYERS ? first[i] : second[i - GROUND_FIRST_PICTURE_LAYERS];
    if (i < terrainLayers) drawn += weights[i];
  }
  // Weights only on layers the terrain has no pictures for: the first layer covers the spot.
  if (drawn < GROUND_WEIGHT_FLOOR) weights[0] = 1.0;
}

// The ground here: the layers' samples mixed by their weights, bent toward the height-based mix
// by each layer's sharpness.
struct Ground {
  vec3 albedo;
  vec3 normal;
  float roughness;
  float occlusion;
};

Ground groundAt(vec3 n) {
  float weights[MAX_TERRAIN_LAYERS];
  groundWeights(weights);
  vec3 dpx = dFdx(fragPosition);
  vec3 dpy = dFdy(fragPosition);
  GroundSample samples[MAX_TERRAIN_LAYERS];
  float top = -1.0;
  for (int i = 0; i < terrainLayers; ++i) {
    if (weights[i] < GROUND_WEIGHT_FLOOR) continue;
    samples[i] = groundLayer(i, n, dpx, dpy);
    top = max(top, weights[i] + samples[i].color.a);
  }
  Ground ground = Ground(vec3(0.0), vec3(0.0), 0.0, 0.0);
  float total = 0.0;
  for (int i = 0; i < terrainLayers; ++i) {
    if (weights[i] < GROUND_WEIGHT_FLOOR) continue;
    float byHeight = max(weights[i] + samples[i].color.a - top + GROUND_HEIGHT_DEPTH, 0.0) / GROUND_HEIGHT_DEPTH;
    float share = mix(weights[i], byHeight * weights[i], terrainSharpness[i]);
    ground.albedo += samples[i].color.rgb * share;
    ground.normal += samples[i].normal * share;
    ground.roughness += samples[i].surface.x * share;
    ground.occlusion += samples[i].surface.y * share;
    total += share;
  }
  float scale = 1.0 / max(total, EPSILON);
  return Ground(ground.albedo * scale, normalize(ground.normal + n * EPSILON), ground.roughness * scale, ground.occlusion * scale);
}
#endif
)GLSL";

// The part above, with the engine's limit on a terrain's layers before it.
inline std::string lit_terrain() {
  return "#ifdef TERRAIN\nconst int MAX_TERRAIN_LAYERS = " + std::to_string(kMaxTerrainLayers) + ";\n#endif\n" +
         kLitTerrain;
}

}  // namespace redgem::shaders
