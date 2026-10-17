#pragma once

// The fragment shader every surface of a scene is lit by. It is one GLSL program, kept in parts
// that share its uniforms and constants: this head (what it takes, how light meets a surface),
// the sun's shadows (lit_shadows.hpp), the light from all around (lit_sky.hpp) and its main
// (lit_main.hpp). lit_fragment() puts them together in that order, with copies fading by distance
// (copy_fade.hpp) and which texels are drawn after the head.

#include <string>

#include "render/shaders/copy_fade.hpp"
#include "render/shaders/lit_main.hpp"
#include "render/shaders/lit_shadows.hpp"
#include "render/shaders/lit_sky.hpp"

namespace redgem::shaders {

// Physically based shading (GGX / Smith / Schlick, glTF metallic-roughness) with a sun, the
// nearest point lights, light from the sky all around (a gradient, or a sky picture), emission
// (factor x map) and exponential fog. Normal maps (no tangents needed: the frame comes from
// screen-space derivatives), roughness/metallic maps (glTF: G and B), occlusion maps (R) and soft
// sun shadows. Base colors from glTF are linear; textures are sRGB. The output is linear HDR:
// exposure, bloom, tone mapping and gamma happen in the post-processing passes.
inline constexpr const char* kLitHead = R"GLSL(
#version 330
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
flat in vec3 fragTint;             // per copy of the mesh, multiplies the base color
flat in vec4 fragCopy;                 // per copy: how much is kept as it fades out (x) and in (y), how much it is lit as facing up (z)
flat in vec3 fragUp;                   // the copy's own up, in the world

uniform sampler2D texture0;
uniform sampler2D emissiveMap;
uniform float hasEmissiveMap;
uniform sampler2D normalMap;
uniform float hasNormalMap;
uniform float normalScale;
uniform sampler2D surfaceMap;      // glTF metallic-roughness: G = roughness, B = metallic
uniform float hasSurfaceMap;
uniform sampler2D occlusionMap;    // R = ambient occlusion
uniform float occlusionStrength;   // 0 when the material has no occlusion map
uniform vec4 colDiffuse;
uniform float metallic;
uniform float roughness;
uniform vec3 emissive;
uniform float alphaCutoff;
uniform float alphaToCoverage;     // 1: cut-out edges are smoothed by the picture's samples
uniform float alphaCeiling;        // at or above: not drawn (see material_alpha.hpp)
uniform float alphaDithered;       // on: a fine pattern of the alpha leaves texels out instead of the cutoff
uniform float alphaMixed;          // on: mixed with what is behind by its alpha
uniform vec3 viewPos;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform int pointLightCount;
uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
uniform float pointLightRanges[MAX_POINT_LIGHTS];
uniform vec3 fogColor;
uniform float fogDensity;

out vec4 finalColor;

const float PI = 3.14159265;
const float TWO_PI = 6.28318531;
const float GAMMA = 2.2;
const float MIN_ROUGHNESS = 0.045;
const float DIELECTRIC_F0 = 0.04;
const float EPSILON = 0.0001;
const float SMITH_K_DIVISOR = 8.0;       // Schlick-GGX k for direct lighting: (r + 1)^2 / 8
const float FRESNEL_POWER = 5.0;
const float LIGHT_FALLOFF_BIAS = 1.0;    // meters added to the distance so lights do not blow up up close
const float SWITCH_ON = 0.5;             // switches arrive as floats: on when above this
const float HALF = 0.5;                  // -1..1 becomes 0..1 when halved and moved up by a half
const float NORMAL_MAP_RANGE = 2.0;      // a normal map stores -1..1 as 0..1
const float SPECULAR_DIVISOR = 4.0;      // Cook-Torrance: D G F / (4 nv nl)

float distributionGGX(float nh, float a) {
  float a2 = a * a;
  float d = nh * nh * (a2 - 1.0) + 1.0;
  return a2 / (PI * d * d + EPSILON);
}

float geometrySmith(float nv, float nl, float rough) {
  float k = (rough + 1.0) * (rough + 1.0) / SMITH_K_DIVISOR;
  return (nv / (nv * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
}

// (Clamped: rounding can push the cosine of two unit vectors past 1, and pow of a negative is undefined.)
vec3 fresnel(float cosTheta, vec3 f0) { return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), FRESNEL_POWER); }

vec3 shade(vec3 n, vec3 v, vec3 l, vec3 radiance, vec3 albedo, vec3 f0, float rough, float metal) {
  vec3 h = normalize(v + l + vec3(EPSILON));  // never the zero vector (light exactly behind the eye)
  float nl = max(dot(n, l), 0.0);
  float nv = max(dot(n, v), EPSILON);
  vec3 f = fresnel(max(dot(h, v), 0.0), f0);
  float a = rough * rough;
  vec3 specular = distributionGGX(max(dot(n, h), 0.0), a) * geometrySmith(nv, nl, rough) * f / (SPECULAR_DIVISOR * nv * nl + EPSILON);
  vec3 diffuse = (1.0 - f) * (1.0 - metal) * albedo / PI;
  return (diffuse + specular) * radiance * nl;
}

// Bends the surface normal by a tangent-space normal map, building the tangent frame from the
// screen-space derivatives of position and UV.
vec3 mappedNormal(vec3 n, vec3 position, vec2 uv) {
  vec3 detail = texture(normalMap, uv).xyz * NORMAL_MAP_RANGE - 1.0;
  detail.xy *= normalScale;
  vec3 dp1 = dFdx(position);
  vec3 dp2 = dFdy(position);
  vec2 duv1 = dFdx(uv);
  vec2 duv2 = dFdy(uv);
  vec3 dp2perp = cross(dp2, n);
  vec3 dp1perp = cross(n, dp1);
  vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
  float scale = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), EPSILON));
  return normalize(mat3(tangent * scale, bitangent * scale, n) * detail);
}

)GLSL";

// What of a surface's texels the lit shader draws (after copy_fade.hpp).
inline constexpr const char* kLitAlpha = R"GLSL(
// How opaque a cut-out pixel is when the picture's samples smooth its edge: the edge sharpened to
// about a pixel wide.
float coverage(float alpha) { return (alpha - alphaCutoff) / max(fwidth(alpha), EPSILON) + HALF; }

// Whether a texel is left out (see material_alpha.hpp): too opaque for the see-through pass, under
// the cutoff, or, dithered, under the pixel's noise.
bool leftOut(float alpha) {
  if (alpha >= alphaCeiling) return true;
  return alphaDithered > SWITCH_ON ? alpha <= pixelNoise() : alpha < alphaCutoff;
}
)GLSL";

// What the lit shader is compiled with besides what every scene needs.
struct LitFeatures {
  bool sky_picture = false;   // the sky is a picture that lights the scene
  bool soft_shadows = false;  // shadow edges as wide as the sun makes them

  bool operator==(const LitFeatures& other) const {
    return sky_picture == other.sky_picture && soft_shadows == other.soft_shadows;
  }
  bool operator!=(const LitFeatures& other) const { return !(*this == other); }
};

inline constexpr const char* kVersionLine = "#version 330\n";
inline constexpr const char* kSkyPictureSwitch = "#define SKY_PICTURE\n";
inline constexpr const char* kSoftShadowsSwitch = "#define SOFT_SHADOWS\n";

// The whole fragment shader, its parts in order, with the features asked for switched on right
// after its version line.
inline std::string lit_fragment(const LitFeatures& features = {}) {
  std::string switches;
  if (features.sky_picture) switches += kSkyPictureSwitch;
  if (features.soft_shadows) switches += kSoftShadowsSwitch;
  std::string source = std::string(kLitHead) + kCopyFade + kLitAlpha + kLitShadows + kLitSky + kLitMain;
  const std::size_t version = source.find(kVersionLine);
  return source.insert(version + std::string(kVersionLine).size(), switches);
}

}  // namespace redgem::shaders
