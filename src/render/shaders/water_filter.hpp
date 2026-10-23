#pragma once

// Fragment-only filtering: the geometric wave shape and its motion stay unchanged.
namespace redgem::shaders {
inline constexpr const char* kWaterFilter = R"GLSL(
const float WATER_NORMAL_FILTER_START = 0.5;  // unwrapped phase radians per pixel
const float WATER_NORMAL_FILTER_END = 2.0;
const float WATER_SLOPE_MEAN_SQUARE = 0.5;
struct WaterWaveNormal {
  vec2 slope;
  float variance;
};
WaterWaveNormal waterFilteredWave(vec2 p, float clock, vec4 component, vec2 phases) {
  float k = WATER_TAU / max(waterWaves.y * component.z, WATER_MIN_LENGTH);
  vec3 shape = waterPhase(p, component.xy, k);
  float phase = shape.x + phases.x - sqrt(WATER_GRAVITY * k) * clock * waterWaves.z;
  vec3 envelope = waterEnvelope(p, phases.y);
  float resolved = 1.0 - smoothstep(WATER_NORMAL_FILTER_START, WATER_NORMAL_FILTER_END, fwidth(phase));
  float amplitude = waterWaves.x * component.w;
  vec2 slope = amplitude * (envelope.x * cos(phase) * shape.yz + sin(phase) * envelope.yz);
  float energy = WATER_SLOPE_MEAN_SQUARE * amplitude * amplitude *
    (envelope.x * envelope.x * dot(shape.yz, shape.yz) + dot(envelope.yz, envelope.yz));
  return WaterWaveNormal(slope * resolved, energy * (1.0 - resolved * resolved));
}
WaterWaveNormal waterAddNormals(WaterWaveNormal a, WaterWaveNormal b) {
  return WaterWaveNormal(a.slope + b.slope, a.variance + b.variance);
}
const float WATER_DETAIL_CLOCK = 0.4;
WaterWaveNormal waterFilteredDetailWave(vec2 p, float clock, vec2 direction, float wavelength, float weight) {
  float k = WATER_TAU / wavelength;
  vec3 shape = waterPhase(p, direction, k);
  float phase = shape.x - sqrt(WATER_GRAVITY * k) * clock * waterWaves.z * WATER_DETAIL_CLOCK;
  float resolved = 1.0 - smoothstep(WATER_NORMAL_FILTER_START, WATER_NORMAL_FILTER_END, fwidth(phase));
  vec2 gradient = shape.yz / k;
  vec2 slope = gradient * (cos(phase) * weight * resolved);
  float energy = WATER_SLOPE_MEAN_SQUARE * weight * weight * dot(gradient, gradient);
  return WaterWaveNormal(slope, energy * (1.0 - resolved * resolved));
}
WaterWaveNormal waterFilteredDetailShape(vec2 p, float clock) {
  WaterWaveNormal detail = waterFilteredDetailWave(p, clock, vec2(0.96, 0.28), 0.38, 0.4);
  detail = waterAddNormals(detail, waterFilteredDetailWave(p, clock, vec2(0.8, 0.6), 0.19, 0.3));
  detail = waterAddNormals(detail, waterFilteredDetailWave(p, clock, vec2(0.6, -0.8), 0.083, 0.2));
  return waterAddNormals(detail, waterFilteredDetailWave(p, clock, vec2(-0.28, 0.96), 0.041, 0.1));
}
WaterWaveNormal waterFilteredShape(vec2 p, float clock) {
  WaterWaveNormal result = waterFilteredWave(p, clock, WATER_COMPONENT_0, WATER_PHASE_0);
  result = waterAddNormals(result, waterFilteredWave(p, clock, WATER_COMPONENT_1, WATER_PHASE_1));
  result = waterAddNormals(result, waterFilteredWave(p, clock, WATER_COMPONENT_2, WATER_PHASE_2));
  result = waterAddNormals(result, waterFilteredWave(p, clock, WATER_COMPONENT_3, WATER_PHASE_3));
  result = waterAddNormals(result, waterFilteredWave(p, clock, WATER_COMPONENT_4, WATER_PHASE_4));
  result = waterAddNormals(result, waterFilteredWave(p, clock, WATER_COMPONENT_5, WATER_PHASE_5));
  return waterAddNormals(result, waterFilteredWave(p, clock, WATER_COMPONENT_6, WATER_PHASE_6));
}
)GLSL";
}  // namespace redgem::shaders
