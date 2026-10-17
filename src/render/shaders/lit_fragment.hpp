#pragma once

// One fragment shader, the one every surface of a scene is lit by. It is long because it is one
// GLSL program: its parts (lights, shadows, reflections, fog) share its uniforms and constants.

namespace redgem::shaders {

// Physically based shading (GGX / Smith / Schlick, glTF metallic-roughness) with a sun, the
// nearest point lights, a sky-gradient ambient + reflection, emission (factor x map) and
// exponential fog. Normal maps (no tangents needed: the frame comes from screen-space derivatives),
// roughness/metallic maps (glTF: G and B), occlusion maps (R) and soft sun shadows. Base colors from glTF are linear;
// textures are sRGB. The output is linear HDR: exposure, bloom, tone mapping and gamma happen in the post-processing
// passes.
inline constexpr const char* kLitFragment = R"GLSL(
#version 330
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
flat in vec3 fragTint;             // per copy of the mesh, multiplies the base color

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
uniform sampler2D reflectionMap;   // the surroundings as a panorama (longitude x latitude), HDR, with mips
uniform float reflectionStrength;  // 0 until the panorama has been captured
uniform sampler2D shadowMap;       // depth of the scene seen from the sun
uniform mat4 lightViewProjection;
uniform float shadowStrength;      // 0 = no shadows
uniform float shadowTexel;         // size of one shadow map texel in UV
uniform float shadowDepthRange;    // meters between the near and far planes of the sun's view
uniform vec4 colDiffuse;
uniform float metallic;
uniform float roughness;
uniform vec3 emissive;
uniform float alphaCutoff;
uniform vec3 viewPos;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float ambientStrength;
uniform vec3 skyTop;
uniform vec3 skyHorizon;
uniform vec3 groundColor;
uniform int pointLightCount;
uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
uniform float pointLightRanges[MAX_POINT_LIGHTS];
uniform vec3 fogColor;
uniform float fogDensity;

out vec4 finalColor;

const float PI = 3.14159265;
const float GAMMA = 2.2;
const float MIN_ROUGHNESS = 0.045;
const float DIELECTRIC_F0 = 0.04;
const float HORIZON_BLEND = 0.25;        // softness of the ground/sky seam in the ambient
const float ZENITH_CURVE = 0.6;          // how quickly the horizon color turns into the zenith color
const float ROUGH_SKY_BLUR = 0.7;
const float EPSILON = 0.0001;
const float SMITH_K_DIVISOR = 8.0;       // Schlick-GGX k for direct lighting: (r + 1)^2 / 8
const float FRESNEL_POWER = 5.0;
const float ROUGH_SPECULAR_DAMPING = 0.5;
const float LIGHT_FALLOFF_BIAS = 1.0;    // meters added to the distance so lights do not blow up up close
const float SHADOW_NORMAL_OFFSET = 0.06; // meters the lookup moves off the surface (prevents acne)
const float SHADOW_MIN_BIAS = 0.02;      // meters
const float SHADOW_SLOPE_BIAS = 0.12;    // meters more on surfaces edge-on to the sun
const int SHADOW_KERNEL = 2;             // (2k+1)^2 taps for soft edges
const float SHADOW_FADE = 0.08;          // fraction of the map's border where shadows fade out
const float MIN_OCCLUSION = 0.0;
const float REFLECTION_MAX_LOD = 6.0;    // blurriest level used, for fully rough surfaces
const float REFLECTION_DIFFUSE_LOD = 8.0;
const float REFLECTION_DIFFUSE_MIX = 0.5; // how much the surroundings (not just the sky) color the ambient light
const float MAX_REFLECTION = 12.0;       // the sun's disc must not turn glossy surfaces into mirrors of it
const float TWO_PI = 6.28318531;
const float SWITCH_ON = 0.5;             // switches arrive as floats: on when above this
const float HALF = 0.5;                  // -1..1 becomes 0..1 when halved and moved up by a half
const float NORMAL_MAP_RANGE = 2.0;      // a normal map stores -1..1 as 0..1
const float SPECULAR_DIVISOR = 4.0;      // Cook-Torrance: D G F / (4 nv nl)
const int SHADOW_TAPS_ACROSS = SHADOW_KERNEL + 1 + SHADOW_KERNEL;  // the texel itself and its neighbours on both sides

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

// How much of the sun reaches a point: 1 = fully lit, 0 = in shadow. Percentage-closer filtering
// softens the edges; outside the shadow map everything is lit.
float sunVisibility(vec3 position, vec3 n, vec3 toSun) {
  if (shadowStrength <= 0.0) return 1.0;
  vec4 clip = lightViewProjection * vec4(position + n * SHADOW_NORMAL_OFFSET, 1.0);
  vec3 coords = clip.xyz / clip.w * HALF + HALF;
  if (coords.z > 1.0) return 1.0;
  vec2 border = min(coords.xy, 1.0 - coords.xy);
  float inside = smoothstep(0.0, SHADOW_FADE, min(border.x, border.y));
  if (inside <= 0.0) return 1.0;
  float bias = max(SHADOW_SLOPE_BIAS * (1.0 - max(dot(n, toSun), 0.0)), SHADOW_MIN_BIAS) / shadowDepthRange;
  float lit = 0.0;
  for (int x = -SHADOW_KERNEL; x <= SHADOW_KERNEL; ++x) {
    for (int y = -SHADOW_KERNEL; y <= SHADOW_KERNEL; ++y) {
      float nearest = texture(shadowMap, coords.xy + vec2(x, y) * shadowTexel).r;
      lit += coords.z - bias > nearest ? 0.0 : 1.0;
    }
  }
  float taps = float(SHADOW_TAPS_ACROSS * SHADOW_TAPS_ACROSS);
  return mix(1.0, lit / taps, shadowStrength * inside);
}

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

void main() {
  vec4 texel = texture(texture0, fragTexCoord);
  float alpha = colDiffuse.a * texel.a * fragColor.a;
  if (alpha < alphaCutoff) discard;
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

  // Ambient: diffuse from the sky around the normal, specular from the sky in the mirror direction.
  vec3 f = fresnel(max(dot(n, v), 0.0), f0);
  vec3 mirror = reflect(-v, n);
  vec3 fromAround = sky(n, ROUGH_SKY_BLUR);
  vec3 reflected = sky(mirror, rough * ROUGH_SKY_BLUR);
  if (reflectionStrength > 0.0) {
    fromAround = mix(fromAround, surroundings(n, REFLECTION_DIFFUSE_LOD), REFLECTION_DIFFUSE_MIX * reflectionStrength);
    reflected = mix(reflected, surroundings(mirror, rough * REFLECTION_MAX_LOD), reflectionStrength);
  }
  vec3 diffuseAmbient = (1.0 - f) * (1.0 - metal) * albedo * fromAround;
  vec3 specularAmbient = f * reflected * (1.0 - rough * ROUGH_SPECULAR_DAMPING);
  float occlusion = mix(1.0, max(texture(occlusionMap, fragTexCoord).r, MIN_OCCLUSION), occlusionStrength);
  color += (diffuseAmbient + specularAmbient) * ambientStrength * occlusion;

  vec3 glow = emissive;
  if (hasEmissiveMap > SWITCH_ON) glow *= pow(texture(emissiveMap, fragTexCoord).rgb, vec3(GAMMA));
  color += glow;

  float fog = 1.0 - exp(-fogDensity * length(viewPos - fragPosition));
  color = mix(color, fogColor, clamp(fog, 0.0, 1.0));
  finalColor = vec4(color, 1.0);
}
)GLSL";

}  // namespace redgem::shaders
