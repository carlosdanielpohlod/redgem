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

// Ages are simulation snapshots, independent of the render clock and water's wind speed.
const int WATER_RIPPLE_CAPACITY = 16;
const int WATER_PREVIOUS_RIPPLE_CAPACITY = 32;  // last drawn impulses plus newly born impulses
uniform int waterRippleCount;
uniform vec4 waterRipples[WATER_RIPPLE_CAPACITY];
uniform float waterRadii[WATER_RIPPLE_CAPACITY];
uniform int previousWaterRippleCount;
uniform vec4 previousWaterRipples[WATER_PREVIOUS_RIPPLE_CAPACITY];
uniform float previousWaterRadii[WATER_PREVIOUS_RIPPLE_CAPACITY];
const float WATER_RIPPLE_WAVELENGTH = 0.65;  // world meters; matches the simulation's ripple model
const float WATER_RIPPLE_SPEED = 1.8;        // world meters per simulation second
const float WATER_RIPPLE_DAMPING = 1.5;      // exponential amplitude loss per simulation second
const float WATER_RIPPLE_MIN_WIDTH = 0.15;  // the smallest footprint accepted by the simulation
const float WATER_RIPPLE_CENTER = 0.05;     // smooth radial distance keeps the center gradient finite
const float WATER_GAUSSIAN_HALF = 0.5;
const float WATER_RIPPLE_TAIL = 8.0;  // discard invisible Gaussian tails before distant-coordinate phase overflow

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
vec3 waterImpulse(vec2 p, vec4 wave, float radius) {
  vec2 offset = p - wave.xy;
  float radial = sqrt(dot(offset, offset) + WATER_RIPPLE_CENTER * WATER_RIPPLE_CENTER);
  float delta = radial - WATER_RIPPLE_CENTER - WATER_RIPPLE_SPEED * wave.z;
  float width = max(radius, WATER_RIPPLE_MIN_WIDTH);
  float scaled = delta / width;
  if (abs(scaled) > WATER_RIPPLE_TAIL) return vec3(0.0);
  float envelope = wave.w * exp(-WATER_RIPPLE_DAMPING * wave.z) * exp(-WATER_GAUSSIAN_HALF * scaled * scaled);
  float k = WATER_TAU / WATER_RIPPLE_WAVELENGTH;
  float phase = k * delta;
  float height = envelope * cos(phase);
  float slope = envelope * (-delta / (width * width) * cos(phase) - k * sin(phase));
  return vec3(height, slope * offset / radial);
}
vec3 waterCurrentRipples(vec2 p) {
  vec3 shape = vec3(0.0);
  for (int i = 0; i < waterRippleCount; ++i) {
    shape += waterImpulse(p, waterRipples[i], waterRadii[i]);
  }
  return shape;
}
vec3 waterPreviousRipples(vec2 p) {
  vec3 shape = vec3(0.0);
  for (int i = 0; i < previousWaterRippleCount; ++i) {
    shape += waterImpulse(p, previousWaterRipples[i], previousWaterRadii[i]);
  }
  return shape;
}
vec3 waterDeformation(vec2 p, float clock, bool previous) {
  vec3 shape = waterShape(p, clock);
  int count = previous ? previousWaterRippleCount : waterRippleCount;
  if (count == 0) return shape;
  return shape + (previous ? waterPreviousRipples(p) : waterCurrentRipples(p));
}

)GLSL";
}  // namespace redgem::shaders
