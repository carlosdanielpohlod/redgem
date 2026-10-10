#pragma once

namespace redgem::shaders {

// Uniform names shared between the GLSL below and the renderer.
inline constexpr const char* kSunDirection = "sunDirection";
inline constexpr const char* kSunColor = "sunColor";
inline constexpr const char* kAmbientStrength = "ambientStrength";
inline constexpr const char* kPointLightCount = "pointLightCount";
inline constexpr const char* kPointLightPositions = "pointLightPositions";
inline constexpr const char* kPointLightColors = "pointLightColors";
inline constexpr const char* kPointLightRanges = "pointLightRanges";
inline constexpr const char* kFogColor = "fogColor";
inline constexpr const char* kFogDensity = "fogDensity";
inline constexpr const char* kViewPosition = "viewPos";
inline constexpr const char* kMetallic = "metallic";
inline constexpr const char* kRoughness = "roughness";
inline constexpr const char* kEmissive = "emissive";
inline constexpr const char* kEmissiveMap = "emissiveMap";
inline constexpr const char* kHasEmissiveMap = "hasEmissiveMap";
inline constexpr const char* kNormalMap = "normalMap";
inline constexpr const char* kHasNormalMap = "hasNormalMap";
inline constexpr const char* kNormalScale = "normalScale";
inline constexpr const char* kSurfaceMap = "surfaceMap";
inline constexpr const char* kHasSurfaceMap = "hasSurfaceMap";
inline constexpr const char* kOcclusionMap = "occlusionMap";
inline constexpr const char* kOcclusionStrength = "occlusionStrength";
inline constexpr const char* kShadowMap = "shadowMap";
inline constexpr const char* kLightViewProjection = "lightViewProjection";
inline constexpr const char* kShadowStrength = "shadowStrength";
inline constexpr const char* kShadowTexel = "shadowTexel";
inline constexpr const char* kShadowDepthRange = "shadowDepthRange";
inline constexpr const char* kTint = "tint";
inline constexpr const char* kBoneMatrices = "boneMatrices";
inline constexpr const char* kSkinned = "skinned";
inline constexpr const char* kReflectionMap = "reflectionMap";
inline constexpr const char* kReflectionStrength = "reflectionStrength";
inline constexpr const char* kFaceForward = "faceForward";
inline constexpr const char* kFaceRight = "faceRight";
inline constexpr const char* kFaceUp = "faceUp";
inline constexpr const char* kTargetSize = "targetSize";
inline constexpr const char* kAlphaCutoff = "alphaCutoff";
inline constexpr const char* kWindAmount = "windAmount";
inline constexpr const char* kTime = "time";
inline constexpr const char* kSkyTop = "skyTop";
inline constexpr const char* kSkyHorizon = "skyHorizon";
inline constexpr const char* kGroundColor = "groundColor";
inline constexpr const char* kSunGlow = "sunGlow";
inline constexpr const char* kModelMatrix = "matModel";
inline constexpr const char* kNormalMatrix = "matNormal";
// Post-processing.
inline constexpr const char* kThreshold = "threshold";
inline constexpr const char* kBlurStep = "blurStep";
inline constexpr const char* kBloomTexture = "bloomTexture";
inline constexpr const char* kBloomStrength = "bloomStrength";
inline constexpr const char* kExposure = "exposure";
inline constexpr const char* kVignette = "vignette";
inline constexpr const char* kTexelSize = "texelSize";
inline constexpr const char* kTanHalfFov = "tanHalfFov";
inline constexpr const char* kAspect = "aspect";
inline constexpr const char* kNearPlane = "nearPlane";
inline constexpr const char* kFarPlane = "farPlane";
inline constexpr const char* kOcclusionTexture = "occlusionTexture";
inline constexpr const char* kOcclusionAmount = "occlusionAmount";

// Must match MAX_POINT_LIGHTS in the lit fragment shader.
inline constexpr int kMaxPointLights = 16;

// Foliage (windAmount > 0) sways: higher parts move more, each tree with its own phase.
// Skinned meshes (skinned > 0.5) are placed by their skeleton: each vertex follows up to four
// bones, whose matrices already lead to world space. MAX_BONES must match config::kMaxSkinJoints.
inline constexpr const char* kLitVertex = R"GLSL(
#version 330
#define MAX_BONES 64
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
uniform mat4 boneMatrices[MAX_BONES];
uniform float skinned;

uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform float windAmount;
uniform float time;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

const float WIND_SPEED = 1.3;
const float WIND_FREQUENCY = 0.35;      // phase change per meter, so neighbours are out of step
const float WIND_GUST_SPEED = 2.9;
const float WIND_GUST = 0.4;
const float WIND_HEIGHT_FULL = 6.0;     // meters above the ground where the sway is strongest
const float WIND_REACH = 0.12;          // meters of sway at full height and wind 1
const float WIND_LIFT = 0.3;            // vertical share of the motion

void main() {
  mat4 toWorld = matModel;
  mat3 normalToWorld = mat3(matNormal);
  if (skinned > 0.5) {
    toWorld = boneMatrices[int(vertexBoneIds.x)] * vertexBoneWeights.x + boneMatrices[int(vertexBoneIds.y)] * vertexBoneWeights.y +
              boneMatrices[int(vertexBoneIds.z)] * vertexBoneWeights.z + boneMatrices[int(vertexBoneIds.w)] * vertexBoneWeights.w;
    normalToWorld = mat3(toWorld);  // good enough for normals: they are normalized, and rigs are not sheared
  }
  vec4 world = toWorld * vec4(vertexPosition, 1.0);
  if (windAmount > 0.0) {
    float phase = time * WIND_SPEED + (world.x + world.z) * WIND_FREQUENCY;
    float sway = sin(phase) + WIND_GUST * sin(time * WIND_GUST_SPEED + world.x);
    float reach = windAmount * WIND_REACH * clamp(world.y / WIND_HEIGHT_FULL, 0.0, 1.0);
    world.x += sway * reach;
    world.z += cos(phase) * reach;
    world.y += sway * reach * WIND_LIFT;
  }
  fragPosition = world.xyz;
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;
  fragNormal = normalize(normalToWorld * vertexNormal);
  gl_Position = matProjection * matView * world;
}
)GLSL";

// Physically based shading (GGX / Smith / Schlick, glTF metallic-roughness) with a sun, the
// nearest point lights, a sky-gradient ambient + reflection, emission (factor x map) and
// exponential fog. Normal maps (no tangents needed: the frame comes from screen-space derivatives),
// roughness/metallic maps (glTF: G and B), occlusion maps (R) and soft sun shadows. Base colors from glTF are linear; textures are sRGB. The output is linear HDR:
// exposure, bloom, tone mapping and gamma happen in the post-processing passes.
inline constexpr const char* kLitFragment = R"GLSL(
#version 330
#define MAX_POINT_LIGHTS 16
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

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
uniform vec3 tint;                 // per object, multiplies the base color
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
  vec3 specular = distributionGGX(max(dot(n, h), 0.0), a) * geometrySmith(nv, nl, rough) * f / (4.0 * nv * nl + EPSILON);
  vec3 diffuse = (1.0 - f) * (1.0 - metal) * albedo / PI;
  return (diffuse + specular) * radiance * nl;
}

// Bends the surface normal by a tangent-space normal map, building the tangent frame from the
// screen-space derivatives of position and UV.
vec3 mappedNormal(vec3 n, vec3 position, vec2 uv) {
  vec3 detail = texture(normalMap, uv).xyz * 2.0 - 1.0;
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
  vec3 coords = clip.xyz / clip.w * 0.5 + 0.5;
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
  float taps = float((2 * SHADOW_KERNEL + 1) * (2 * SHADOW_KERNEL + 1));
  return mix(1.0, lit / taps, shadowStrength * inside);
}

// Where a direction falls on the panorama. Must match the capture (kPanoramaFragment).
vec2 panoramaUv(vec3 d) {
  return vec2(atan(d.x, -d.z) / TWO_PI + 0.5, asin(clamp(d.y, -1.0, 1.0)) / PI + 0.5);
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
  vec3 albedo = pow(texel.rgb, vec3(GAMMA)) * colDiffuse.rgb * fragColor.rgb * tint;
  float rough = roughness;
  float metal = metallic;
  if (hasSurfaceMap > 0.5) {
    vec3 surface = texture(surfaceMap, fragTexCoord).rgb;
    rough *= surface.g;
    metal *= surface.b;
  }
  rough = clamp(rough, MIN_ROUGHNESS, 1.0);
  metal = clamp(metal, 0.0, 1.0);
  vec3 n = normalize(fragNormal);
  if (!gl_FrontFacing) n = -n;  // double-sided surfaces (leaves) are lit on both sides
  vec3 flatNormal = n;
  if (hasNormalMap > 0.5) n = mappedNormal(n, fragPosition, fragTexCoord);
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
  if (hasEmissiveMap > 0.5) glow *= pow(texture(emissiveMap, fragTexCoord).rgb, vec3(GAMMA));
  color += glow;

  float fog = 1.0 - exp(-fogDensity * length(viewPos - fragPosition));
  color = mix(color, fogColor, clamp(fog, 0.0, 1.0));
  finalColor = vec4(color, 1.0);
}
)GLSL";

// Depth-only pass for the shadow map: same vertices as the lit pass (wind included), and cut-out
// materials leave holes.
inline constexpr const char* kDepthFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float alphaCutoff;
void main() {
  if (colDiffuse.a * texture(texture0, fragTexCoord).a * fragColor.a < alphaCutoff) discard;
}
)GLSL";

// The sky dome: drawn around the camera before everything else.
inline constexpr const char* kSkyVertex = R"GLSL(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
out vec3 direction;
void main() {
  direction = vertexPosition;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

inline constexpr const char* kSkyFragment = R"GLSL(
#version 330
in vec3 direction;
uniform vec3 skyTop;
uniform vec3 skyHorizon;
uniform vec3 groundColor;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunGlow;
out vec4 finalColor;

const float ZENITH_CURVE = 0.6;    // the same curve the lit shader uses for ambient light
const float GROUND_BLEND = 0.6;
const float GROUND_TINT = 0.25;
const float DISC_SHARPNESS = 1500.0;   // the sun's disc
const float DISC_BRIGHTNESS = 12.0;
const float HALO_SHARPNESS = 24.0;     // the glow around it
const float HALO_BRIGHTNESS = 0.9;
const float HAZE_SHARPNESS = 3.0;      // the warm side of the sky
const float HAZE_BRIGHTNESS = 0.25;
const float EPSILON = 0.0001;

void main() {
  vec3 d = normalize(direction);
  vec3 color = mix(skyHorizon, skyTop, pow(clamp(d.y, 0.0, 1.0), ZENITH_CURVE));
  // Below the horizon the dome keeps the horizon color, slightly tinted by the ground far down.
  color = mix(color, mix(skyHorizon, groundColor, GROUND_TINT), smoothstep(0.0, -GROUND_BLEND, d.y));
  float toSun = max(dot(d, normalize(-sunDirection)), 0.0);
  vec3 tint = sunColor / (max(sunColor.r, max(sunColor.g, sunColor.b)) + EPSILON);
  color += tint * sunGlow * (pow(toSun, DISC_SHARPNESS) * DISC_BRIGHTNESS + pow(toSun, HALO_SHARPNESS) * HALO_BRIGHTNESS +
                             pow(toSun, HAZE_SHARPNESS) * HAZE_BRIGHTNESS);
  finalColor = vec4(color, 1.0);
}
)GLSL";

// Copies one face of a cube capture (a 90-degree view along faceForward) into the part of the
// panorama it covers; the other pixels are left as they were.
inline constexpr const char* kPanoramaFragment = R"GLSL(
#version 330
uniform sampler2D texture0;  // the face
uniform vec3 faceForward;
uniform vec3 faceRight;
uniform vec3 faceUp;
uniform vec2 targetSize;     // of the panorama, in pixels
out vec4 finalColor;
const float PI = 3.14159265;
const float TWO_PI = 6.28318531;
const float FACE_EDGE = 1.0;
void main() {
  vec2 uv = gl_FragCoord.xy / targetSize;
  float longitude = (uv.x - 0.5) * TWO_PI;
  float latitude = (uv.y - 0.5) * PI;
  vec3 d = vec3(cos(latitude) * sin(longitude), sin(latitude), -cos(latitude) * cos(longitude));
  float along = dot(d, faceForward);
  if (along <= 0.0) discard;
  vec2 onFace = vec2(dot(d, faceRight), dot(d, faceUp)) / along;
  if (max(abs(onFace.x), abs(onFace.y)) > FACE_EDGE) discard;
  finalColor = vec4(texture(texture0, onFace * 0.5 + 0.5).rgb, 1.0);
}
)GLSL";

// Full-screen passes use raylib's default vertex shader (fragTexCoord, fragColor).

// Screen-space ambient occlusion from the scene's depth: how enclosed each pixel is by the
// geometry around it (white = open, dark = in a crease or under something).
inline constexpr const char* kAmbientOcclusionFragment = R"GLSL(
#version 330
uniform sampler2D texture0;  // scene depth
uniform vec2 targetSize;
uniform float tanHalfFov;
uniform float aspect;
uniform float nearPlane;
uniform float farPlane;
out vec4 finalColor;
const int SAMPLES = 14;
const float RADIUS = 0.7;            // meters around the surface that can occlude it
const float BIAS = 0.04;             // meters: keeps flat surfaces from occluding themselves
const float GOLDEN_ANGLE = 2.39996323;
const float TWO_PI = 6.28318531;
const float INNER_REACH = 0.25;      // the first samples stay this close (fraction of the radius)
const float SKY_DEPTH = 0.99999;
const float FAR_FADE_START = 60.0;   // meters: the effect fades out with distance (it gets noisy)
const float FAR_FADE_END = 110.0;

float viewDepth(vec2 uv) {
  float ndc = texture(texture0, uv).r * 2.0 - 1.0;
  return 2.0 * nearPlane * farPlane / (farPlane + nearPlane - ndc * (farPlane - nearPlane));
}

vec3 viewPosition(vec2 uv) {
  float depth = viewDepth(uv);
  return vec3((uv * 2.0 - 1.0) * vec2(tanHalfFov * aspect, tanHalfFov) * depth, -depth);
}

float noise(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void main() {
  vec2 uv = gl_FragCoord.xy / targetSize;
  if (texture(texture0, uv).r >= SKY_DEPTH) {
    finalColor = vec4(1.0);
    return;
  }
  vec3 position = viewPosition(uv);
  vec3 normal = normalize(cross(dFdx(position), dFdy(position)));
  if (normal.z < 0.0) normal = -normal;  // toward the camera
  vec3 tangent = normalize(abs(normal.y) < 0.99 ? cross(normal, vec3(0.0, 1.0, 0.0)) : cross(normal, vec3(1.0, 0.0, 0.0)));
  vec3 bitangent = cross(normal, tangent);
  float spin = noise(gl_FragCoord.xy) * TWO_PI;
  float occluded = 0.0;
  for (int i = 0; i < SAMPLES; ++i) {
    float fraction = (float(i) + 0.5) / float(SAMPLES);
    float ring = sqrt(fraction);
    float angle = spin + float(i) * GOLDEN_ANGLE;
    vec3 hemisphere = vec3(cos(angle) * ring, sin(angle) * ring, sqrt(1.0 - fraction));
    vec3 probe = position + (tangent * hemisphere.x + bitangent * hemisphere.y + normal * hemisphere.z) *
                                RADIUS * mix(INNER_REACH, 1.0, fraction);
    if (probe.z > -nearPlane) continue;  // behind the camera: nothing to compare with
    vec2 probeUv = probe.xy / (-probe.z * vec2(tanHalfFov * aspect, tanHalfFov)) * 0.5 + 0.5;
    float sceneZ = -viewDepth(probeUv);
    float inRange = smoothstep(0.0, 1.0, RADIUS / max(abs(position.z - sceneZ), 0.0001));
    occluded += (sceneZ >= probe.z + BIAS ? 1.0 : 0.0) * inRange;
  }
  float open = 1.0 - occluded / float(SAMPLES);
  open = mix(open, 1.0, smoothstep(FAR_FADE_START, FAR_FADE_END, -position.z));
  finalColor = vec4(vec3(open), 1.0);
}
)GLSL";

// Keeps what is brighter than the threshold (drawn into a smaller target: the bloom source).
inline constexpr const char* kBrightFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float threshold;
out vec4 finalColor;
const float KNEE = 0.6;         // soft start above the threshold
const float MAX_BRIGHT = 8.0;   // tiny mirror-like highlights must not make the glow flicker
void main() {
  vec3 color = texture(texture0, fragTexCoord).rgb;
  // A broken pixel (not a number) must not be smeared over the screen by the blur.
  if (any(isnan(color)) || any(isinf(color))) color = vec3(0.0);
  color = min(color, vec3(MAX_BRIGHT));
  float brightest = max(color.r, max(color.g, color.b));
  finalColor = vec4(color * smoothstep(threshold, threshold + KNEE, brightest), 1.0);
}
)GLSL";

// One direction of a separable Gaussian blur (9 taps using bilinear filtering).
inline constexpr const char* kBlurFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 blurStep;  // one texel along the blur direction
out vec4 finalColor;
const float OFFSET_1 = 1.3846153846;
const float OFFSET_2 = 3.2307692308;
const float WEIGHT_0 = 0.2270270270;
const float WEIGHT_1 = 0.3162162162;
const float WEIGHT_2 = 0.0702702703;
void main() {
  vec3 color = texture(texture0, fragTexCoord).rgb * WEIGHT_0;
  color += texture(texture0, fragTexCoord + blurStep * OFFSET_1).rgb * WEIGHT_1;
  color += texture(texture0, fragTexCoord - blurStep * OFFSET_1).rgb * WEIGHT_1;
  color += texture(texture0, fragTexCoord + blurStep * OFFSET_2).rgb * WEIGHT_2;
  color += texture(texture0, fragTexCoord - blurStep * OFFSET_2).rgb * WEIGHT_2;
  finalColor = vec4(color, 1.0);
}
)GLSL";

// HDR scene + bloom -> exposure, filmic tone mapping (ACES fit), vignette, gamma.
// Alpha carries the luma, which the anti-aliasing pass reads.
inline constexpr const char* kCompositeFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform float bloomStrength;
uniform sampler2D occlusionTexture;  // ambient occlusion: 1 = open
uniform float occlusionAmount;
uniform float exposure;
uniform float vignette;
out vec4 finalColor;
const float GAMMA = 2.2;
const float ACES_A = 2.51;
const float ACES_B = 0.03;
const float ACES_C = 2.43;
const float ACES_D = 0.59;
const float ACES_E = 0.14;
const float VIGNETTE_START = 0.35;  // distance from the center where the darkening begins
const float VIGNETTE_END = 1.0;
const vec3 LUMA = vec3(0.299, 0.587, 0.114);
void main() {
  vec3 scene = texture(texture0, fragTexCoord).rgb * mix(1.0, texture(occlusionTexture, fragTexCoord).r, occlusionAmount);
  vec3 color = (scene + texture(bloomTexture, fragTexCoord).rgb * bloomStrength) * exposure;
  color = clamp((color * (ACES_A * color + ACES_B)) / (color * (ACES_C * color + ACES_D) + ACES_E), 0.0, 1.0);
  float fromCenter = length(fragTexCoord - vec2(0.5)) * 2.0;
  color *= 1.0 - vignette * smoothstep(VIGNETTE_START, VIGNETTE_END + VIGNETTE_START, fromCenter);
  color = pow(color, vec3(1.0 / GAMMA));
  finalColor = vec4(color, dot(color, LUMA));
}
)GLSL";

// FXAA (after Timothy Lottes' FXAA 3.11, console quality): smooths edges using the luma in alpha.
inline constexpr const char* kAntialiasFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 texelSize;
out vec4 finalColor;
const float EDGE_THRESHOLD = 0.125;     // local contrast needed to treat a pixel as an edge
const float EDGE_THRESHOLD_MIN = 0.0312;
const float SPAN_MAX = 8.0;             // longest blur along an edge, in texels
const float REDUCE_MUL = 0.125;
const float REDUCE_MIN = 0.0078125;
const float NEAR_TAP = 0.1666667;       // 1/3 - 1/2
const float FAR_TAP = 0.5;
void main() {
  vec4 center = texture(texture0, fragTexCoord);
  float lumaNW = texture(texture0, fragTexCoord + vec2(-1.0, -1.0) * texelSize).a;
  float lumaNE = texture(texture0, fragTexCoord + vec2(1.0, -1.0) * texelSize).a;
  float lumaSW = texture(texture0, fragTexCoord + vec2(-1.0, 1.0) * texelSize).a;
  float lumaSE = texture(texture0, fragTexCoord + vec2(1.0, 1.0) * texelSize).a;
  float lumaMin = min(center.a, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
  float lumaMax = max(center.a, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
  if (lumaMax - lumaMin < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD)) {
    finalColor = vec4(center.rgb, 1.0);
    return;
  }
  vec2 dir = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), (lumaNW + lumaSW) - (lumaNE + lumaSE));
  float reduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * REDUCE_MUL, REDUCE_MIN);
  float scale = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
  dir = clamp(dir * scale, vec2(-SPAN_MAX), vec2(SPAN_MAX)) * texelSize;
  vec4 near = 0.5 * (texture(texture0, fragTexCoord + dir * -NEAR_TAP) + texture(texture0, fragTexCoord + dir * NEAR_TAP));
  vec4 far = 0.5 * near + 0.25 * (texture(texture0, fragTexCoord + dir * -FAR_TAP) + texture(texture0, fragTexCoord + dir * FAR_TAP));
  finalColor = vec4((far.a < lumaMin || far.a > lumaMax) ? near.rgb : far.rgb, 1.0);
}
)GLSL";

}  // namespace redgem::shaders
