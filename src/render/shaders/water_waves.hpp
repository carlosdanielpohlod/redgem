#pragma once

// A sum of traveling waves, in world space: changing mesh density does not change the pattern.
namespace redgem::shaders {
inline constexpr const char* kWaterWaves = R"GLSL(
uniform float waterEnabled;
uniform vec3 waterWaves;  // maximum height, dominant wavelength, clock multiplier
const float WATER_TAU = 6.28318531;
const float WATER_GRAVITY = 9.81;
const float WATER_MIN_LENGTH = 0.05;
const vec2 WATER_DIRECTION_A = vec2(0.8, 0.6);
const vec2 WATER_DIRECTION_B = vec2(-0.6, 0.8);
const vec2 WATER_DIRECTION_C = vec2(0.28, -0.96);
const vec3 WATER_LENGTHS = vec3(1.0, 0.47, 0.21);
const vec3 WATER_WEIGHTS = vec3(0.6, 0.28, 0.12);  // sum 1 bounds displacement by wave height
const float WATER_PHASE_BEND = 0.8;  // radians: break the infinite parallel crests of a plane wave
const float WATER_BEND_FREQUENCY = 0.31;

// Phase and its spatial gradient; the same curvature drives displacement and shading.
vec3 waterPhase(vec2 p, vec2 direction, float k) {
  vec2 across = vec2(-direction.y, direction.x);
  float bend = dot(p, across) * k * WATER_BEND_FREQUENCY;
  float phase = dot(p, direction) * k + WATER_PHASE_BEND * sin(bend);
  vec2 gradient = k * (direction + across * WATER_PHASE_BEND * WATER_BEND_FREQUENCY * cos(bend));
  return vec3(phase, gradient);
}

// Height and the two partial derivatives, used to recover the normal without finite differences.
vec3 waterWave(vec2 p, float clock, vec2 direction, float lengthScale, float weight) {
  float k = WATER_TAU / max(waterWaves.y * lengthScale, WATER_MIN_LENGTH);
  vec3 shape = waterPhase(p, direction, k);
  float phase = shape.x - sqrt(WATER_GRAVITY * k) * clock * waterWaves.z;
  float height = waterWaves.x * weight;
  return vec3(height * sin(phase), height * cos(phase) * shape.yz);
}
vec3 waterShape(vec2 p, float clock) {
  return waterWave(p, clock, WATER_DIRECTION_A, WATER_LENGTHS.x, WATER_WEIGHTS.x)
       + waterWave(p, clock, WATER_DIRECTION_B, WATER_LENGTHS.y, WATER_WEIGHTS.y)
       + waterWave(p, clock, WATER_DIRECTION_C, WATER_LENGTHS.z, WATER_WEIGHTS.z);
}
)GLSL";
}  // namespace redgem::shaders
