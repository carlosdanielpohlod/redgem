#pragma once

// A full-screen pass: raylib's default vertex shader feeds it (fragTexCoord, fragColor).

namespace redgem::shaders {

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
const float HALF = 0.5;              // -1..1 becomes 0..1 when halved and moved up by a half
const float CLIP_RANGE = 2.0;        // 0..1 becomes -1..1 when doubled and moved down by one
const float LINEAR_DEPTH_SCALE = 2.0;  // of a perspective view: depth = 2 n f / (f + n - z (f - n))
const float SAMPLE_MIDDLE = 0.5;     // each sample sits in the middle of its share of the disc
const float NEARLY_VERTICAL = 0.99;  // |y| of a normal beyond which Y cannot give it a tangent
const float MIN_DEPTH_GAP = 0.0001;  // meters: keeps the range test from dividing by zero
const vec2 NOISE_DIRECTION = vec2(12.9898, 78.233);  // the usual one-line hash: a sine too fast to follow
const float NOISE_SCALE = 43758.5453;

float viewDepth(vec2 uv) {
  float ndc = texture(texture0, uv).r * CLIP_RANGE - 1.0;
  return LINEAR_DEPTH_SCALE * nearPlane * farPlane / (farPlane + nearPlane - ndc * (farPlane - nearPlane));
}

vec3 viewPosition(vec2 uv) {
  float depth = viewDepth(uv);
  return vec3((uv * CLIP_RANGE - 1.0) * vec2(tanHalfFov * aspect, tanHalfFov) * depth, -depth);
}

float noise(vec2 p) { return fract(sin(dot(p, NOISE_DIRECTION)) * NOISE_SCALE); }

void main() {
  vec2 uv = gl_FragCoord.xy / targetSize;
  if (texture(texture0, uv).r >= SKY_DEPTH) {
    finalColor = vec4(1.0);
    return;
  }
  vec3 position = viewPosition(uv);
  vec3 normal = normalize(cross(dFdx(position), dFdy(position)));
  if (normal.z < 0.0) normal = -normal;  // toward the camera
  vec3 tangent = normalize(abs(normal.y) < NEARLY_VERTICAL ? cross(normal, vec3(0.0, 1.0, 0.0)) : cross(normal, vec3(1.0, 0.0, 0.0)));
  vec3 bitangent = cross(normal, tangent);
  float spin = noise(gl_FragCoord.xy) * TWO_PI;
  float occluded = 0.0;
  for (int i = 0; i < SAMPLES; ++i) {
    float fraction = (float(i) + SAMPLE_MIDDLE) / float(SAMPLES);
    float ring = sqrt(fraction);
    float angle = spin + float(i) * GOLDEN_ANGLE;
    vec3 hemisphere = vec3(cos(angle) * ring, sin(angle) * ring, sqrt(1.0 - fraction));
    vec3 probe = position + (tangent * hemisphere.x + bitangent * hemisphere.y + normal * hemisphere.z) *
                                RADIUS * mix(INNER_REACH, 1.0, fraction);
    if (probe.z > -nearPlane) continue;  // behind the camera: nothing to compare with
    vec2 probeUv = probe.xy / (-probe.z * vec2(tanHalfFov * aspect, tanHalfFov)) * HALF + HALF;
    float sceneZ = -viewDepth(probeUv);
    float inRange = smoothstep(0.0, 1.0, RADIUS / max(abs(position.z - sceneZ), MIN_DEPTH_GAP));
    occluded += (sceneZ >= probe.z + BIAS ? 1.0 : 0.0) * inRange;
  }
  float open = 1.0 - occluded / float(SAMPLES);
  open = mix(open, 1.0, smoothstep(FAR_FADE_START, FAR_FADE_END, -position.z));
  finalColor = vec4(vec3(open), 1.0);
}
)GLSL";

}  // namespace redgem::shaders
