#pragma once

// Refraction reads a separate snapshot of opaque HDR color and depth, never the active target.
namespace redgem::shaders {
inline constexpr const char* kLitWater = R"GLSL(
#ifdef WATER
uniform sampler2D waterBackground;
uniform sampler2D waterDepth;
uniform float waterReady;
uniform vec3 waterAbsorption;
uniform vec2 waterSurface;  // local horizontal shore width in meters, refraction strength
uniform vec2 waterPlanes;
uniform float time;
uniform mat4 matView;
uniform mat4 matProjection;
const float WATER_IOR = 1.333;
const float WATER_REFRACTION_SCALE = 0.035;  // default preserves the Snell-law projection
const float WATER_F0 = 0.02037;      // air/water interface, IOR 1.333
const float WATER_WIND_SCALE = 10.0;
const float WATER_MAX_RIPPLE_SLOPE = 0.35;
const float WATER_NORMAL_VARIANCE = 0.25;
const float WATER_UNRESOLVED_ROUGHNESS_LIMIT = 0.25;
const float WATER_DEPTH_LIMIT = 100.0;  // meters: no unbounded absorption path on sky pixels
const float WATER_FOAM_ROUGHNESS = 0.8;

float waterLinearDepth(float z) {
  float ndc = z * 2.0 - 1.0;
  return 2.0 * waterPlanes.x * waterPlanes.y /
    (waterPlanes.y + waterPlanes.x - ndc * (waterPlanes.y - waterPlanes.x));
}
struct WaterNormal {
  vec3 direction;
  float variance;
};
WaterNormal waterNormal() {
  vec2 p = fragPosition.xz;
  float wind = clamp(waterWaves.x / max(waterWaves.y, WATER_MIN_LENGTH) * WATER_WIND_SCALE, 0.0, WATER_MAX_RIPPLE_SLOPE);
  WaterWaveNormal detail = waterFilteredDetailShape(p, time);
  WaterWaveNormal macro = waterFilteredShape(p, time);
  vec2 slope = macro.slope + waterCurrentRipples(p).yz + detail.slope * wind;
  vec3 normal = normalize(vec3(-slope.x, 1.0, -slope.y));
  return WaterNormal(gl_FrontFacing ? normal : -normal, macro.variance + detail.variance * wind * wind);
}
float waterRoughness(WaterNormal normal) {
  vec3 dx = dFdx(normal.direction);
  vec3 dy = dFdy(normal.direction);
  float variance = max(dot(dx, dx), dot(dy, dy)) * WATER_NORMAL_VARIANCE;
  variance += min(normal.variance * WATER_NORMAL_VARIANCE, WATER_UNRESOLVED_ROUGHNESS_LIMIT);
  return clamp(sqrt(roughness * roughness + variance), MIN_ROUGHNESS, 1.0);
}
// Bounded secant iterations intersect the refracted ray with the visible depth field.
// A miss keeps the undistorted opaque sample: hidden and off-screen surfaces are unavailable.
const int WATER_REFRACTION_STEPS = 8;
const int WATER_REFRACTION_SEARCH_STEPS = 32;  // recovery only: failed secants or nonplanar depth boundaries
const int WATER_REFRACTION_REFINEMENT = 8;
const float WATER_REFRACTION_DEPTH_CURVE = 0.00001;  // hardware-depth precision floor for curvature
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

bool waterInsideScreen(vec2 uv) {
  return all(greaterThan(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0)));
}
bool waterScreenPoint(vec3 point, out vec2 uv) {
  if (point.z >= -waterPlanes.x) return false;
  vec4 clip = matProjection * vec4(point, 1.0);
  uv = clip.xy / clip.w * HALF + HALF;
  return waterInsideScreen(uv);
}
float waterRayGap(vec3 origin, vec3 ray, float distance, out vec2 uv) {
  vec3 point = origin + ray * distance;
  if (!waterScreenPoint(point, uv)) return -WATER_DEPTH_LIMIT;
  return -point.z - waterLinearDepth(texture(waterDepth, uv).r);
}
// Hardware depth is affine across a projected plane, even when its view-space slope is steep.
// Only curvature relative to the local slope triggers point sampling at an occlusion boundary.
bool waterRefractionDepthCurve(float left, float middle, float right) {
  float curvature = abs(left + right - 2.0 * middle);
  float slope = max(abs(left - middle), abs(right - middle));
  return curvature > max(WATER_REFRACTION_DEPTH_CURVE, slope * HALF);
}
float waterRefractionTexel(ivec2 pixel) {
  ivec2 size = textureSize(waterDepth, 0);
  return texelFetch(waterDepth, clamp(pixel, ivec2(0), size - 1), 0).r;
}
bool waterRefractionDiagonalCurve(ivec2 pixel, float middle) {
  float upperLeft = waterRefractionTexel(pixel + ivec2(-1, -1));
  float lowerRight = waterRefractionTexel(pixel + ivec2(1, 1));
  float upperRight = waterRefractionTexel(pixel + ivec2(1, -1));
  float lowerLeft = waterRefractionTexel(pixel + ivec2(-1, 1));
  return waterRefractionDepthCurve(upperLeft, middle, lowerRight) ||
    waterRefractionDepthCurve(upperRight, middle, lowerLeft);
}
bool waterRefractionDiscontinuous(vec2 uv) {
  ivec2 pixel = ivec2(uv * vec2(textureSize(waterDepth, 0)));
  float middle = waterRefractionTexel(pixel);
  float left = waterRefractionTexel(pixel - ivec2(1, 0));
  float right = waterRefractionTexel(pixel + ivec2(1, 0));
  float above = waterRefractionTexel(pixel - ivec2(0, 1));
  float below = waterRefractionTexel(pixel + ivec2(0, 1));
  if (waterRefractionDepthCurve(left, middle, right) || waterRefractionDepthCurve(above, middle, below)) return true;
  return waterRefractionDiagonalCurve(pixel, middle);
}
vec2 waterRefractionColorUv(vec2 uv) {
  vec2 size = vec2(textureSize(waterDepth, 0));
  return (floor(uv * size) + HALF) / size;
}
bool waterRefractionSample(vec3 surface, vec3 ray, float distance, out vec2 uv, out float gap) {
  vec3 point = surface + ray * distance;
  if (!waterScreenPoint(point, uv)) return false;
  float depth = texture(waterDepth, uv).r;
  if (depth >= 1.0 - EPSILON) return false;
  gap = -point.z - waterLinearDepth(depth);
  return true;
}
bool waterRefractionDistanceOutside(float distance) {
  return distance <= 0.0 || distance > WATER_DEPTH_LIMIT;
}
bool waterAdvanceRefraction(float gap, inout float before, inout float previousGap, inout float distance) {
  float difference = gap - previousGap;
  if (abs(difference) <= EPSILON) return false;
  float next = distance - gap * (distance - before) / difference;
  before = distance;
  previousGap = gap;
  distance = next;
  return true;
}
float waterRefractionTolerance(float depth) {
  return max(WATER_REFRACTION_THICKNESS, depth * WATER_REFRACTION_RELATIVE_THICKNESS);
}
bool waterAcceptRefractionEstimate(vec2 hit, float distance, out vec3 result) {
  result = vec3(hit, distance);
  return !waterRefractionDiscontinuous(hit);
}
bool waterEstimateRefraction(vec3 surface, vec3 ray, float column, float behind, out vec3 result) {
  float before = 0.0;
  float previousGap = -column;
  float distance = column / -ray.z;
  vec2 hit;
  float gap;
  for (int i = 0; i < WATER_REFRACTION_STEPS; ++i) {
    if (waterRefractionDistanceOutside(distance)) break;
    if (!waterRefractionSample(surface, ray, distance, hit, gap)) break;
    float thickness = waterRefractionTolerance(behind);
    if (abs(gap) <= thickness) return waterAcceptRefractionEstimate(hit, distance, result);
    if (!waterAdvanceRefraction(gap, before, previousGap, distance)) break;
  }
  return false;
}
bool waterRefractionPointSample(vec3 surface, vec3 ray, float distance, out vec2 uv, out float gap) {
  vec3 point = surface + ray * distance;
  if (!waterScreenPoint(point, uv)) return false;
  ivec2 size = textureSize(waterDepth, 0);
  float depth = texelFetch(waterDepth, clamp(ivec2(uv * vec2(size)), ivec2(0), size - 1), 0).r;
  if (depth >= 1.0 - EPSILON) return false;
  gap = -point.z - waterLinearDepth(depth);
  return true;
}
bool waterAcceptRefractionHit(vec3 surface, vec3 ray, float distance, out vec3 hit) {
  vec2 uv;
  float gap;
  if (!waterRefractionSample(surface, ray, distance, uv, gap)) return false;
  bool edge = waterRefractionDiscontinuous(uv);
  if (edge && !waterRefractionPointSample(surface, ray, distance, uv, gap)) return false;
  float tolerance = waterRefractionTolerance(-(surface + ray * distance).z);
  if (abs(gap) > tolerance) return false;
  hit = vec3(edge ? waterRefractionColorUv(uv) : uv, distance);
  return true;
}
bool waterRefineRefraction(vec3 surface, vec3 ray, float before, float after, out vec3 hit) {
  vec2 uv;
  float gap;
  for (int i = 0; i < WATER_REFRACTION_REFINEMENT; ++i) {
    float middle = (before + after) * HALF;
    if (!waterRefractionSample(surface, ray, middle, uv, gap)) return false;
    if (gap >= 0.0) after = middle;
    else before = middle;
  }
  return waterAcceptRefractionHit(surface, ray, after, hit);
}
bool waterRefractionCrossing(float previousGap, float gap) {
  return previousGap < 0.0 && gap >= 0.0;
}
struct WaterRefractionInterval {
  float distance;
  float gap;
};
vec3 waterRefractionViewPoint(vec2 uv) {
  float depth = waterLinearDepth(texture(waterDepth, uv).r);
  vec2 projected = uv * 2.0 - 1.0 + matProjection[2].xy;
  vec2 scale = vec2(matProjection[0].x, matProjection[1].y);
  return vec3(projected * depth / scale, -depth);
}
vec3 waterRefractionBottomNormal(vec2 uv) {
  vec2 pixel = 1.0 / vec2(textureSize(waterDepth, 0));
  if (!waterInsideScreen(uv + pixel) || !waterInsideScreen(uv - pixel)) return vec3(0.0);
  vec2 dx = vec2(pixel.x, 0.0);
  vec2 dy = vec2(0.0, pixel.y);
  vec3 across = (waterRefractionViewPoint(uv + dx) - waterRefractionViewPoint(uv - dx)) / pixel.x;
  vec3 along = (waterRefractionViewPoint(uv + dy) - waterRefractionViewPoint(uv - dy)) / pixel.y;
  vec3 normal = cross(across, along);
  float magnitude = dot(normal, normal);
  return magnitude > EPSILON * EPSILON ? normal * inversesqrt(magnitude) : vec3(0.0);
}
vec3 waterRefractionFallback(vec3 surface, vec3 ray, vec3 fallback) {
  if (waterRefractionDiscontinuous(fallback.xy)) return fallback;
  vec3 bottom = waterRefractionViewPoint(fallback.xy);
  vec3 normal = waterRefractionBottomNormal(fallback.xy);
  float direction = dot(normal, ray);
  if (!(abs(direction) > EPSILON)) return fallback;
  // Hidden geometry has no confirmed hit: use the visible bottom's local tangent plane as a proxy.
  float distance = dot(normal, bottom - surface) / direction;
  if (!(distance >= 0.0 && distance <= WATER_DEPTH_LIMIT)) return fallback;
  return vec3(fallback.xy, distance);
}
struct WaterRefractionScreenRay {
  vec4 origin;
  vec4 direction;
  vec2 from;
  vec2 to;
};
WaterRefractionScreenRay waterRefractionScreenRay(vec3 surface, vec3 ray) {
  vec4 origin = matProjection * vec4(surface, 1.0);
  vec4 direction = matProjection * vec4(ray, 0.0);
  vec4 end = origin + direction * WATER_DEPTH_LIMIT;
  return WaterRefractionScreenRay(origin, direction, origin.xy / origin.w, end.xy / end.w);
}
float waterRefractionScreenDistance(WaterRefractionScreenRay ray, float fraction) {
  vec2 extent = abs(ray.to - ray.from);
  int axis = extent.x > extent.y ? 0 : 1;
  float coordinate = mix(ray.from[axis], ray.to[axis], fraction);
  float denominator = coordinate * ray.direction.w - ray.direction[axis];
  if (abs(denominator) <= EPSILON) return WATER_DEPTH_LIMIT * fraction;
  return (ray.origin[axis] - coordinate * ray.origin.w) / denominator;
}
vec3 waterSearchRefraction(vec3 surface, vec3 ray, float column, vec3 fallback) {
  WaterRefractionScreenRay screenRay = waterRefractionScreenRay(surface, ray);
  WaterRefractionInterval before = WaterRefractionInterval(0.0, -column);
  vec2 uv;
  float gap;
  for (int i = 1; i <= WATER_REFRACTION_SEARCH_STEPS; ++i) {
    float distance = waterRefractionScreenDistance(screenRay, float(i) / float(WATER_REFRACTION_SEARCH_STEPS));
    // Bracket the same continuous depth field used by refinement; point sampling only validates edge hits.
    if (!waterRefractionSample(surface, ray, distance, uv, gap)) return waterRefractionFallback(surface, ray, fallback);
    vec3 hit;
    if (waterRefractionCrossing(before.gap, gap) &&
        waterRefineRefraction(surface, ray, before.distance, distance, hit)) return hit;
    before = WaterRefractionInterval(distance, gap);
  }
  return waterRefractionFallback(surface, ray, fallback);
}
vec3 waterTraceRefraction(vec3 surface, vec3 ray, float column, float behind, vec3 fallback) {
  vec3 estimate;
  if (waterEstimateRefraction(surface, ray, column, behind, estimate)) return estimate;
  return waterSearchRefraction(surface, ray, column, fallback);
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
  return waterTraceRefraction(surface, ray, column, behind, fallback);
}
float waterRefineReflection(vec3 origin, vec3 ray, float before, float after) {
  vec2 uv;
  for (int refine = 0; refine < WATER_REFLECTION_REFINEMENT; ++refine) {
    float middle = (before + after) * HALF;
    if (waterRayGap(origin, ray, middle, uv) > 0.0) after = middle;
    else before = middle;
  }
  return after;
}
bool waterReflectionGapOutside(float gap) {
  return gap < 0.0 || gap > WATER_REFLECTION_THICKNESS;
}
vec4 waterReflectionHit(vec3 origin, vec3 ray, float before, float distance) {
  float after = waterRefineReflection(origin, ray, before, distance);
  vec2 uv;
  float gap = waterRayGap(origin, ray, after, uv);
  if (waterReflectionGapOutside(gap)) return vec4(0.0);
  vec2 edge = min(uv, 1.0 - uv);
  float confidence = smoothstep(0.0, WATER_REFLECTION_EDGE, min(edge.x, edge.y));
  vec4 reflected = waterReflectedFog(texture(waterBackground, uv).rgb, origin + ray * after, after);
  return vec4(reflected.rgb, confidence * reflected.a);
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
    if (gap >= 0.0) return waterReflectionHit(origin, ray, before, distance);
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
vec3 waterAmbientIrradiance(vec3 up) {
#ifdef SKY_PICTURE
  vec3 around = pictureIrradiance(up);
#else
  vec3 around = sky(up, ROUGH_SKY_BLUR);
#endif
  if (reflectionStrength > 0.0)
    around = mix(around, surroundings(up, REFLECTION_DIFFUSE_LOD), REFLECTION_DIFFUSE_MIX * reflectionStrength);
  return around;
}
vec3 waterIrradiance() {
  const vec3 up = vec3(0.0, 1.0, 0.0);
  vec3 around = waterAmbientIrradiance(up);
  vec3 sun = normalize(-sunDirection);
  vec3 light = around * ambientStrength + sunColor * max(sun.y, 0.0) * sunVisibility(fragPosition, up, sun);
  for (int i = 0; i < pointLightCount; ++i) {
    float distance = length(pointLightPositions[i] - fragPosition);
    float window = clamp(1.0 - distance / pointLightRanges[i], 0.0, 1.0);
    light += pointLightColors[i] * window * window / (distance * distance + LIGHT_FALLOFF_BIAS);
  }
  return light;
}
vec3 waterPointReflection(vec3 n, vec3 v, vec3 f0, float rough, int index) {
  vec3 toLight = pointLightPositions[index] - fragPosition;
  float distance = length(toLight);
  float window = clamp(1.0 - distance / pointLightRanges[index], 0.0, 1.0);
  float falloff = window * window / (distance * distance + LIGHT_FALLOFF_BIAS);
  return shade(n, v, toLight / max(distance, EPSILON), pointLightColors[index] * falloff * PI,
    vec3(0.0), f0, rough, 1.0);
}
vec3 waterReflection(vec3 n, vec3 v, float rough) {
  vec3 f0 = vec3(WATER_F0);
  vec3 reflected = waterAmbientReflection(n, v, rough);
  vec3 sun = normalize(-sunDirection);
  reflected += shade(n, v, sun, sunColor * PI, vec3(0.0), f0, rough, 1.0)
    * sunVisibility(fragPosition, vec3(0.0, 1.0, 0.0), sun);
  for (int i = 0; i < pointLightCount; ++i) {
    reflected += waterPointReflection(n, v, f0, rough, i);
  }
  return reflected;
}
struct WaterTransmission {
  vec3 background;
  vec3 transmission;
  float thickness;
};
struct WaterSurfaceLight {
  vec3 transmitted;
  vec3 surface;
};
bool waterTransmittedHit(vec3 hit, float bottom) {
  return hit.z >= 0.0 && bottom < 1.0 - EPSILON;
}
WaterTransmission waterSampleTransmission(vec3 n, vec3 v) {
  vec2 uv = gl_FragCoord.xy / vec2(textureSize(waterBackground, 0));
  vec3 ray = refract(normalize(mat3(matView) * -v), normalize(mat3(matView) * n), 1.0 / WATER_IOR);
  vec3 hit = waterUv(uv, ray, gl_FragCoord.z);
  float bottom = texture(waterDepth, hit.xy).r;
  float thickness = hit.z >= 0.0 ? hit.z : WATER_DEPTH_LIMIT;
  bool transmittedHit = waterTransmittedHit(hit, bottom);
  vec3 transmission = transmittedHit ? exp(-max(waterAbsorption, vec3(0.0)) * thickness) : vec3(0.0);
  vec3 background = texture(waterBackground, hit.xy).rgb;
  return WaterTransmission(background, transmission, thickness);
}
WaterSurfaceLight waterSurfaceLight(vec3 n, vec3 v, vec3 transmission, float rough) {
  vec3 scattered = colDiffuse.rgb * waterIrradiance() * (1.0 - transmission);
  float fresnelWeight = fresnel(max(dot(n, v), 0.0), vec3(WATER_F0)).x;
  vec3 transmitted = transmission * (1.0 - fresnelWeight);
  vec3 surface = scattered * (1.0 - fresnelWeight) + waterReflection(n, v, rough);
  return WaterSurfaceLight(transmitted, surface);
}
vec3 waterFoamLight(vec3 n, vec3 v) {
  vec3 light = ambientLight(n, v, vec3(1.0), vec3(DIELECTRIC_F0), WATER_FOAM_ROUGHNESS, 0.0) * ambientStrength;
  vec3 toSun = normalize(-sunDirection);
  light += shade(n, v, toSun, sunColor * PI, vec3(1.0), vec3(DIELECTRIC_F0), WATER_FOAM_ROUGHNESS, 0.0)
    * sunVisibility(fragPosition, vec3(0.0, 1.0, 0.0), toSun);
  return light;
}
// Local horizontal distance to the intersection of the water and visible-bottom tangent planes.
const float WATER_FOAM_MIN_SURFACE_UP = 0.02;  // reject surfaces that cannot be represented as height fields
const float WATER_FOAM_MIN_SHORE_GRADIENT = 0.005;  // almost parallel planes do not establish a visible shore

bool waterFoamSurfaceSlope(vec3 geometric, out vec2 slope) {
  float magnitude = length(geometric);
  bool degenerate = abs(geometric.y) <= magnitude * WATER_FOAM_MIN_SURFACE_UP;
  if (degenerate) return false;
  slope = -geometric.xz / geometric.y;
  return true;
}

bool waterFoamNeighborsInside(vec2 uv, vec2 pixel) {
  return waterInsideScreen(uv + pixel) && waterInsideScreen(uv - pixel);
}

bool waterFoamBottomKnown(vec2 uv) {
  vec2 pixel = 1.0 / vec2(textureSize(waterDepth, 0));
  if (!waterFoamNeighborsInside(uv, pixel)) return false;
  if (texture(waterDepth, uv).r >= 1.0 - EPSILON) return false;
  return !waterRefractionDiscontinuous(uv);
}

float waterFoamPlaneDistance(vec3 surface, vec3 bottom, vec3 normal, vec2 slope) {
  vec3 worldNormal = transpose(mat3(matView)) * normal;
  vec2 gradient = worldNormal.xz + worldNormal.y * slope;
  float magnitude = length(gradient);
  if (magnitude <= WATER_FOAM_MIN_SHORE_GRADIENT) return -1.0;
  return abs(dot(normal, surface - bottom)) / magnitude;
}

float waterFoamShoreDistance(vec3 geometric) {
  if (waterSurface.x <= 0.0) return -1.0;
  vec2 slope;
  if (!waterFoamSurfaceSlope(geometric, slope)) return -1.0;
  vec2 uv = gl_FragCoord.xy / vec2(textureSize(waterBackground, 0));
  if (!waterFoamBottomKnown(uv)) return -1.0;
  vec3 surface = (matView * vec4(fragPosition, 1.0)).xyz;
  vec3 bottom = waterRefractionViewPoint(uv);
  vec3 normal = waterRefractionBottomNormal(uv);
  return waterFoamPlaneDistance(surface, bottom, normal, slope);
}

// Foam is a continuous world-space band, rather than a uniform white intersection line.
const vec2 WATER_FOAM_HASH_DIRECTION = vec2(127.1, 311.7);
const float WATER_FOAM_HASH_SCALE = 43758.5453;
const float WATER_FOAM_PATCH_FREQUENCY = 0.7;  // inverse meters: broad shore patches
const float WATER_FOAM_GRAIN_FREQUENCY = 32.0;  // inverse meters: centimeter-scale porous coverage
const vec2 WATER_FOAM_GRAIN_OFFSET = vec2(7.3, -2.8);
const float WATER_FOAM_MIN_REACH = 0.85;        // shares of the authored nominal horizontal width
const float WATER_FOAM_MAX_REACH = 1.15;
const float WATER_FOAM_PORE_START = 0.35;  // low-density pockets leave the underlying water visible
const float WATER_FOAM_PORE_END = 0.65;
const float WATER_FOAM_MAX_COVERAGE = 0.72;  // foam preserves some transmission even in dense patches
const float WATER_FOAM_CONTACT_FADE = 0.12;    // horizontal meters: cap the proportional contact fade
const float WATER_FOAM_CONTACT_SHARE = 0.15;   // narrow authored bands retain a proportionate fade
// Symmetric thresholds and a symmetric noise ensemble give expected coverage exactly one half.
const float WATER_FOAM_MEAN_POROSITY = 0.5;
const float WATER_FOAM_FILTER_START = 0.25;  // cells per pixel: retain detail with at least four pixels per cell
const float WATER_FOAM_FILTER_END = 1.0;     // unresolved cells converge to expected coverage, never to zero

float waterFoamHash(vec2 p) { return fract(sin(dot(p, WATER_FOAM_HASH_DIRECTION)) * WATER_FOAM_HASH_SCALE); }

float waterFoamNoise(vec2 p) {
  vec2 cell = floor(p);
  vec2 blend = smoothstep(vec2(0.0), vec2(1.0), fract(p));
  float lower = mix(waterFoamHash(cell), waterFoamHash(cell + vec2(1.0, 0.0)), blend.x);
  float upper = mix(waterFoamHash(cell + vec2(0.0, 1.0)), waterFoamHash(cell + vec2(1.0)), blend.x);
  return mix(lower, upper, blend.y);
}

float waterFoamPores(vec2 p, float footprint) {
  float grain = waterFoamNoise(p);
  float coverage = smoothstep(WATER_FOAM_PORE_START, WATER_FOAM_PORE_END, grain);
  float resolved = 1.0 - smoothstep(WATER_FOAM_FILTER_START, WATER_FOAM_FILTER_END, footprint);
  return mix(WATER_FOAM_MEAN_POROSITY, coverage, resolved);
}

bool waterFoamUnsupported(float distance) {
  return waterSurface.x <= 0.0 || distance < 0.0;
}

float waterFoamCoverage(float distance, float footprint) {
  if (waterFoamUnsupported(distance)) return 0.0;
  float shorePatch = waterFoamNoise(fragPosition.xz * WATER_FOAM_PATCH_FREQUENCY);
  float pores = waterFoamPores(fragPosition.xz * WATER_FOAM_GRAIN_FREQUENCY + WATER_FOAM_GRAIN_OFFSET,
                               footprint * WATER_FOAM_GRAIN_FREQUENCY);
  float reach = waterSurface.x * mix(WATER_FOAM_MIN_REACH, WATER_FOAM_MAX_REACH, shorePatch);
  float contact = min(WATER_FOAM_CONTACT_FADE, waterSurface.x * WATER_FOAM_CONTACT_SHARE);
  float band = smoothstep(0.0, contact, distance) * (1.0 - smoothstep(0.0, reach, distance));
  return band * WATER_FOAM_MAX_COVERAGE * pores;
}

vec3 waterCompose(vec3 n, vec3 v, WaterTransmission optics, WaterSurfaceLight light,
                  float shoreDistance, float foamFootprint) {
  float foam = waterFoamCoverage(shoreDistance, foamFootprint);
  vec3 foamLight = waterFoamLight(n, v);
  vec3 surface = mix(light.surface, foamLight, foam);
  vec3 transmitted = light.transmitted;
  transmitted *= 1.0 - foam;
  float air = exp(-fogDensity * length(viewPos - fragPosition));
  // The snapshot already contains atmospheric fog. Only the new surface contribution needs it.
  return optics.background * transmitted + surface * air + fogColor * (1.0 - transmitted) * (1.0 - air);
}
vec3 waterColor() {
  // Evaluate derivatives for all water pixels before depth-dependent guards or traces diverge.
  vec3 geometryDx = dFdx(fragPosition);
  vec3 geometryDy = dFdy(fragPosition);
  float foamFootprint = max(length(geometryDx.xz), length(geometryDy.xz));
  WaterNormal normal = waterNormal();
  float rough = waterRoughness(normal);
  float shoreDistance = waterFoamShoreDistance(cross(geometryDx, geometryDy));
  vec3 n = normal.direction;
  vec3 v = normalize(viewPos - fragPosition);
  WaterTransmission optics = waterSampleTransmission(n, v);
  WaterSurfaceLight light = waterSurfaceLight(n, v, optics.transmission, rough);
  return waterCompose(n, v, optics, light, shoreDistance, foamFootprint);
}
#endif
)GLSL";
}  // namespace redgem::shaders
