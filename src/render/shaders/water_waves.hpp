#pragma once

// A sum of traveling waves, in world space: changing mesh density does not change the pattern.
namespace redgem::shaders {
inline constexpr const char* kWaterWaves = R"GLSL(
uniform float waterEnabled;
uniform vec3 waterWaves;  // maximum height, dominant wavelength, clock multiplier
const float WATER_TAU = 6.28318531;
const float WATER_GRAVITY = 9.81;
const float WATER_MIN_LENGTH = 0.05;
// Direction XZ, relative wavelength, amplitude weight; positive weights sum to one.
const vec4 WATER_COMPONENT_0 = vec4(0.96651104, 0.25662504, 1.000, 0.300);
const vec2 WATER_PHASE_0 = vec2(0.35, 1.20);
const vec4 WATER_COMPONENT_1 = vec4(0.91446668, 0.40466120, 0.790, 0.230);
const vec2 WATER_PHASE_1 = vec2(1.90, 3.40);
const vec4 WATER_COMPONENT_2 = vec4(0.84924920, 0.52799222, 0.610, 0.170);
const vec2 WATER_PHASE_2 = vec2(4.10, 5.10);
const vec4 WATER_COMPONENT_3 = vec4(0.76750205, 0.64104649, 0.430, 0.120);
const vec2 WATER_PHASE_3 = vec2(5.35, 0.45);
const vec4 WATER_COMPONENT_4 = vec4(0.67081635, 0.74162351, 0.310, 0.085);
const vec2 WATER_PHASE_4 = vec2(2.75, 2.60);
const vec4 WATER_COMPONENT_5 = vec4(0.56107397, 0.82776567, 0.227, 0.055);
const vec2 WATER_PHASE_5 = vec2(0.95, 4.30);
const vec4 WATER_COMPONENT_6 = vec4(0.47147527, 0.88187928, 0.163, 0.040);
const vec2 WATER_PHASE_6 = vec2(4.85, 5.90);
const int WATER_WAVE_COUNT = 7;
const vec4 WATER_COMPONENTS[WATER_WAVE_COUNT] = vec4[WATER_WAVE_COUNT](
  WATER_COMPONENT_0, WATER_COMPONENT_1, WATER_COMPONENT_2, WATER_COMPONENT_3,
  WATER_COMPONENT_4, WATER_COMPONENT_5, WATER_COMPONENT_6);
const vec2 WATER_PHASES[WATER_WAVE_COUNT] = vec2[WATER_WAVE_COUNT](
  WATER_PHASE_0, WATER_PHASE_1, WATER_PHASE_2, WATER_PHASE_3, WATER_PHASE_4, WATER_PHASE_5, WATER_PHASE_6);
const float WATER_ENVELOPE_BASE = 0.825;
const float WATER_ENVELOPE_VARIATION = 0.175;  // envelope stays in [0.65, 1], preserving displacement bounds
const float WATER_ENVELOPE_FREQUENCY = 0.23;  // radians per dominant wavelength
const vec2 WATER_ENVELOPE_DIRECTION_A = vec2(0.6, 0.8);
const vec2 WATER_ENVELOPE_DIRECTION_B = vec2(-0.8, 0.6);
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

// A slow spatial envelope breaks continuous equal-height crests without exceeding wave_height.
vec3 waterEnvelope(vec2 p, float phase) {
  float frequency = WATER_ENVELOPE_FREQUENCY / max(waterWaves.y, WATER_MIN_LENGTH);
  float u = dot(p, WATER_ENVELOPE_DIRECTION_A) * frequency + phase;
  float v = dot(p, WATER_ENVELOPE_DIRECTION_B) * frequency - phase;
  float value = WATER_ENVELOPE_BASE + WATER_ENVELOPE_VARIATION * sin(u) * sin(v);
  vec2 gradient = WATER_ENVELOPE_VARIATION * frequency *
    (WATER_ENVELOPE_DIRECTION_A * cos(u) * sin(v) + WATER_ENVELOPE_DIRECTION_B * sin(u) * cos(v));
  return vec3(value, gradient);
}
// One wave at `p`: its phase, the carrier's phase gradient, its envelope (value, then gradient)
// and its height. The displacement and the shaded normals both come from these.
struct WaterWaveTerms {
  float phase;
  vec2 carrier;
  vec3 envelope;
  float height;
};
WaterWaveTerms waterWaveTerms(vec2 p, float clock, vec4 component, vec2 phases) {
  float k = WATER_TAU / max(waterWaves.y * component.z, WATER_MIN_LENGTH);
  vec3 shape = waterPhase(p, component.xy, k);
  float phase = shape.x + phases.x - sqrt(WATER_GRAVITY * k) * clock * waterWaves.z;
  return WaterWaveTerms(phase, shape.yz, waterEnvelope(p, phases.y), waterWaves.x * component.w);
}
// The analytic partial derivatives of a wave's height: both carrier and envelope gradients.
vec2 waterWaveGradient(WaterWaveTerms wave) {
  return wave.height * (wave.envelope.x * cos(wave.phase) * wave.carrier + sin(wave.phase) * wave.envelope.yz);
}
vec3 waterWave(vec2 p, float clock, vec4 component, vec2 phases) {
  WaterWaveTerms wave = waterWaveTerms(p, clock, component, phases);
  return vec3(wave.height * wave.envelope.x * sin(wave.phase), waterWaveGradient(wave));
}
vec3 waterShape(vec2 p, float clock) {
  vec3 shape = vec3(0.0);
  for (int i = 0; i < WATER_WAVE_COUNT; ++i) shape += waterWave(p, clock, WATER_COMPONENTS[i], WATER_PHASES[i]);
  return shape;
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
