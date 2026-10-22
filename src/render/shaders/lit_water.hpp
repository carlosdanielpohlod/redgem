#pragma once

// Refraction reads a separate snapshot of opaque HDR color and depth, never the active target.
namespace redgem::shaders {
inline constexpr const char* kLitWater = R"GLSL(
#ifdef WATER
uniform sampler2D waterBackground;
uniform sampler2D waterDepth;
uniform float waterReady;
uniform vec3 waterAbsorption;
uniform vec2 waterSurface;  // shore foam width in meters, refraction strength
uniform vec2 waterPlanes;
uniform float time;
uniform mat4 matView;
uniform mat4 matProjection;
const float WATER_IOR = 1.333;
const float WATER_REFRACTION_SCALE = 0.035;  // default preserves the Snell-law projection
const float WATER_F0 = 0.02037;      // air/water interface, IOR 1.333
const float WATER_WIND_SCALE = 10.0;
const float WATER_MAX_RIPPLE_SLOPE = 0.35;
const float WATER_RIPPLE_CLOCK = 0.4;
const float WATER_FILTER_START = 0.5;  // phase change per pixel before unresolved detail fades
const float WATER_FILTER_END = 2.0;
const float WATER_NORMAL_VARIANCE = 0.25;
const float WATER_DEPTH_LIMIT = 100.0;  // meters: no unbounded absorption path on sky pixels
const float WATER_FOAM_ROUGHNESS = 0.8;

float waterLinearDepth(float z) {
  float ndc = z * 2.0 - 1.0;
  return 2.0 * waterPlanes.x * waterPlanes.y /
    (waterPlanes.y + waterPlanes.x - ndc * (waterPlanes.y - waterPlanes.x));
}
vec2 waterRipple(vec2 p, vec2 direction, float wavelength, float weight) {
  float k = WATER_TAU / wavelength;
  vec3 shape = waterPhase(p, direction, k);
  float phase = shape.x - sqrt(WATER_GRAVITY * k) * time * waterWaves.z * WATER_RIPPLE_CLOCK;
  float resolved = 1.0 - smoothstep(WATER_FILTER_START, WATER_FILTER_END, fwidth(phase));
  return shape.yz / k * (cos(phase) * weight * resolved);
}
vec3 waterNormal() {
  vec2 p = fragPosition.xz;
  float wind = clamp(waterWaves.x / max(waterWaves.y, WATER_MIN_LENGTH) * WATER_WIND_SCALE, 0.0, WATER_MAX_RIPPLE_SLOPE);
  vec2 detail = waterRipple(p, vec2(0.96, 0.28), 0.38, 0.4)
              + waterRipple(p, vec2(0.8, 0.6), 0.19, 0.3)
              + waterRipple(p, vec2(0.6, -0.8), 0.083, 0.2)
              + waterRipple(p, vec2(-0.28, 0.96), 0.041, 0.1);
  vec2 slope = waterShape(p, time).yz + detail * wind;
  vec3 normal = normalize(vec3(-slope.x, 1.0, -slope.y));
  return gl_FrontFacing ? normal : -normal;
}
float waterRoughness(vec3 n) {
  vec3 dx = dFdx(n);
  vec3 dy = dFdy(n);
  float variance = max(dot(dx, dx), dot(dy, dy)) * WATER_NORMAL_VARIANCE;
  return clamp(sqrt(roughness * roughness + variance), MIN_ROUGHNESS, 1.0);
}
// Bounded secant iterations intersect the refracted ray with the visible depth field.
// A miss keeps the undistorted opaque sample: hidden and off-screen surfaces are unavailable.
const int WATER_REFRACTION_STEPS = 8;
const float WATER_REFRACTION_THICKNESS = 0.02;  // view-space meters near the camera
const float WATER_REFRACTION_RELATIVE_THICKNESS = 0.0005;
// A short view-space ray against the opaque depth snapshot. A miss keeps the sky/probe fallback.
const int WATER_REFLECTION_STEPS = 32;
const int WATER_REFLECTION_REFINEMENT = 5;
const float WATER_REFLECTION_START = 0.12;
const float WATER_REFLECTION_STRIDE = 0.15;
const float WATER_REFLECTION_GROWTH = 0.025;
const float WATER_REFLECTION_THICKNESS = 0.18;
const float WATER_REFLECTION_EDGE = 0.08;
const float WATER_REFLECTION_BIAS = 0.02;
const float WATER_FOG_REJECT = 0.01;  // recovering radiance from an opaque fog sample amplifies quantization
const float WATER_FOG_ACCEPT = 0.1;

vec4 waterReflectedFog(vec3 sampleColor, vec3 point, float distance) {
  float cameraAir = exp(-fogDensity * length(point));
  vec3 radiance = fogColor + (sampleColor - fogColor) / max(cameraAir, WATER_FOG_REJECT);
  vec3 reflected = mix(fogColor, max(radiance, vec3(0.0)), exp(-fogDensity * distance));
  return vec4(reflected, smoothstep(WATER_FOG_REJECT, WATER_FOG_ACCEPT, cameraAir));
}

bool waterScreenPoint(vec3 point, out vec2 uv) {
  if (point.z >= -waterPlanes.x) return false;
  vec4 clip = matProjection * vec4(point, 1.0);
  uv = clip.xy / clip.w * HALF + HALF;
  return all(greaterThan(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0)));
}
float waterRayGap(vec3 origin, vec3 ray, float distance, out vec2 uv) {
  vec3 point = origin + ray * distance;
  if (!waterScreenPoint(point, uv)) return -WATER_DEPTH_LIMIT;
  return -point.z - waterLinearDepth(texture(waterDepth, uv).r);
}
bool waterRefractionSample(vec3 surface, vec3 ray, float distance, out vec2 uv, out float gap) {
  vec3 point = surface + ray * distance;
  if (!waterScreenPoint(point, uv)) return false;
  float depth = texture(waterDepth, uv).r;
  if (depth >= 1.0 - EPSILON) return false;
  gap = -point.z - waterLinearDepth(depth);
  return true;
}
vec3 waterUv(vec2 uv, vec3 snell, float surfaceDepth) {
  vec3 surface = (matView * vec4(fragPosition, 1.0)).xyz;
  float behind = waterLinearDepth(texture(waterDepth, uv).r);
  float column = max(behind - waterLinearDepth(surfaceDepth), 0.0);
  if (column <= EPSILON) return vec3(uv, 0.0);
  vec3 fallback = vec3(uv, clamp(column / max(-snell.z, EPSILON), 0.0, WATER_DEPTH_LIMIT));
  if (waterSurface.y <= 0.0) return fallback;
  vec3 ray = normalize(mix(normalize(surface), snell, waterSurface.y / WATER_REFRACTION_SCALE));
  if (ray.z >= -EPSILON) return fallback;
  float before = 0.0;
  float previousGap = -column;
  float distance = column / -ray.z;
  vec2 hit;
  float gap;
  for (int i = 0; i < WATER_REFRACTION_STEPS; ++i) {
    if (distance <= 0.0 || distance > WATER_DEPTH_LIMIT) break;
    if (!waterRefractionSample(surface, ray, distance, hit, gap)) break;
    float thickness = max(WATER_REFRACTION_THICKNESS, behind * WATER_REFRACTION_RELATIVE_THICKNESS);
    if (abs(gap) <= thickness) return vec3(hit, distance);
    float difference = gap - previousGap;
    if (abs(difference) <= EPSILON) break;
    float next = distance - gap * (distance - before) / difference;
    before = distance;
    previousGap = gap;
    distance = next;
  }
  return fallback;
}
vec4 waterScreenReflection(vec3 n, vec3 v) {
  vec3 origin = (matView * vec4(fragPosition + n * WATER_REFLECTION_BIAS, 1.0)).xyz;
  vec3 ray = normalize(mat3(matView) * reflect(-v, n));
  float before = WATER_REFLECTION_START;
  float distance = before;
  vec2 uv;
  for (int i = 0; i < WATER_REFLECTION_STEPS; ++i) {
    distance += WATER_REFLECTION_STRIDE + float(i) * WATER_REFLECTION_GROWTH;
    if (!waterScreenPoint(origin + ray * distance, uv)) return vec4(0.0);
    float gap = waterRayGap(origin, ray, distance, uv);
    if (gap >= 0.0) {
      float after = distance;
      for (int refine = 0; refine < WATER_REFLECTION_REFINEMENT; ++refine) {
        float middle = (before + after) * HALF;
        if (waterRayGap(origin, ray, middle, uv) > 0.0) after = middle;
        else before = middle;
      }
      gap = waterRayGap(origin, ray, after, uv);
      if (gap < 0.0 || gap > WATER_REFLECTION_THICKNESS) return vec4(0.0);
      vec2 edge = min(uv, 1.0 - uv);
      float confidence = smoothstep(0.0, WATER_REFLECTION_EDGE, min(edge.x, edge.y));
      vec4 reflected = waterReflectedFog(texture(waterBackground, uv).rgb, origin + ray * after, after);
      return vec4(reflected.rgb, confidence * reflected.a);
    }
    before = distance;
  }
  return vec4(0.0);
}
vec3 waterAmbientReflection(vec3 n, vec3 v, float rough) {
  vec3 f0 = vec3(WATER_F0);
  vec3 fallback = ambientLight(n, v, vec3(0.0), f0, rough, 1.0) * ambientStrength;
  vec4 screen = waterScreenReflection(n, v);
  float confidence = screen.a * pow(1.0 - rough, SPECULAR_DIVISOR);
  return mix(fallback, screen.rgb * fresnel(max(dot(n, v), 0.0), f0), confidence);
}
vec3 waterIrradiance() {
  const vec3 up = vec3(0.0, 1.0, 0.0);
#ifdef SKY_PICTURE
  vec3 around = pictureIrradiance(up);
#else
  vec3 around = sky(up, ROUGH_SKY_BLUR);
#endif
  if (reflectionStrength > 0.0)
    around = mix(around, surroundings(up, REFLECTION_DIFFUSE_LOD), REFLECTION_DIFFUSE_MIX * reflectionStrength);
  vec3 sun = normalize(-sunDirection);
  vec3 light = around * ambientStrength + sunColor * max(sun.y, 0.0) * sunVisibility(fragPosition, up, sun);
  for (int i = 0; i < pointLightCount; ++i) {
    float distance = length(pointLightPositions[i] - fragPosition);
    float window = clamp(1.0 - distance / pointLightRanges[i], 0.0, 1.0);
    light += pointLightColors[i] * window * window / (distance * distance + LIGHT_FALLOFF_BIAS);
  }
  return light;
}
vec3 waterReflection(vec3 n, vec3 v, float rough) {
  vec3 f0 = vec3(WATER_F0);
  vec3 reflected = waterAmbientReflection(n, v, rough);
  vec3 sun = normalize(-sunDirection);
  reflected += shade(n, v, sun, sunColor * PI, vec3(0.0), f0, rough, 1.0)
    * sunVisibility(fragPosition, vec3(0.0, 1.0, 0.0), sun);
  for (int i = 0; i < pointLightCount; ++i) {
    vec3 toLight = pointLightPositions[i] - fragPosition;
    float distance = length(toLight);
    float window = clamp(1.0 - distance / pointLightRanges[i], 0.0, 1.0);
    float falloff = window * window / (distance * distance + LIGHT_FALLOFF_BIAS);
    reflected += shade(n, v, toLight / max(distance, EPSILON), pointLightColors[i] * falloff * PI,
      vec3(0.0), f0, rough, 1.0);
  }
  return reflected;
}
vec3 waterColor() {
  vec3 n = waterNormal();
  vec3 v = normalize(viewPos - fragPosition);
  vec2 uv = gl_FragCoord.xy / vec2(textureSize(waterBackground, 0));
  vec3 ray = refract(normalize(mat3(matView) * -v), normalize(mat3(matView) * n), 1.0 / WATER_IOR);
  vec3 hit = waterUv(uv, ray, gl_FragCoord.z);
  float bottom = texture(waterDepth, hit.xy).r;
  float thickness = hit.z >= 0.0 ? hit.z : WATER_DEPTH_LIMIT;
  bool transmittedHit = hit.z >= 0.0 && bottom < 1.0 - EPSILON;
  vec3 transmission = transmittedHit ? exp(-max(waterAbsorption, vec3(0.0)) * thickness) : vec3(0.0);
  vec3 background = texture(waterBackground, hit.xy).rgb;
  vec3 scattered = colDiffuse.rgb * waterIrradiance() * (1.0 - transmission);
  float fresnelWeight = fresnel(max(dot(n, v), 0.0), vec3(WATER_F0)).x;
  vec3 transmitted = transmission * (1.0 - fresnelWeight);
  vec3 surface = scattered * (1.0 - fresnelWeight) + waterReflection(n, v, waterRoughness(n));
  float foam = waterSurface.x > 0.0 ? 1.0 - smoothstep(0.0, waterSurface.x, thickness) : 0.0;
  vec3 foamLight = ambientLight(n, v, vec3(1.0), vec3(DIELECTRIC_F0), WATER_FOAM_ROUGHNESS, 0.0) * ambientStrength;
  vec3 toSun = normalize(-sunDirection);
  foamLight += shade(n, v, toSun, sunColor * PI, vec3(1.0), vec3(DIELECTRIC_F0), WATER_FOAM_ROUGHNESS, 0.0)
    * sunVisibility(fragPosition, vec3(0.0, 1.0, 0.0), toSun);
  surface = mix(surface, foamLight, foam);
  transmitted *= 1.0 - foam;
  float air = exp(-fogDensity * length(viewPos - fragPosition));
  // The snapshot already contains atmospheric fog. Only the new surface contribution needs it.
  return background * transmitted + surface * air + fogColor * (1.0 - transmitted) * (1.0 - air);
}
#endif
)GLSL";
}  // namespace redgem::shaders
