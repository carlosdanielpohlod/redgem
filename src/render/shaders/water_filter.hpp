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
// A wave's slope (the displacement's own gradient, see waterWaveGradient) where the pixel resolves
// its phase; what it cannot resolve becomes slope variance.
WaterWaveNormal waterFilteredWave(vec2 p, float clock, vec4 component, vec2 phases) {
  WaterWaveTerms wave = waterWaveTerms(p, clock, component, phases);
  float resolved = 1.0 - smoothstep(WATER_NORMAL_FILTER_START, WATER_NORMAL_FILTER_END, fwidth(wave.phase));
  vec3 envelope = wave.envelope;
  float energy = WATER_SLOPE_MEAN_SQUARE * wave.height * wave.height *
    (envelope.x * envelope.x * dot(wave.carrier, wave.carrier) + dot(envelope.yz, envelope.yz));
  return WaterWaveNormal(waterWaveGradient(wave) * resolved, energy * (1.0 - resolved * resolved));
}
WaterWaveNormal waterAddNormals(WaterWaveNormal a, WaterWaveNormal b) {
  return WaterWaveNormal(a.slope + b.slope, a.variance + b.variance);
}
// Finite log-band micro-height model. Equal L2 weights preserve the previous 0.30 slope energy.
const float WATER_DETAIL_CLOCK = 0.4;
const int WATER_DETAIL_COUNT = 12;
const vec4 WATER_DETAIL_COMPONENTS[WATER_DETAIL_COUNT] = vec4[WATER_DETAIL_COUNT](
  vec4(0.9939367674, 0.1099531824, 0.5084255560, 0.158113883008),
  vec4(-0.2621564185, 0.9650253946, 0.3756597507, 0.158113883008),
  vec4(0.7183267597, 0.6957058763, 0.3210140430, 0.158113883008),
  vec4(0.9561460767, -0.2928902183, 0.2371872419, 0.158113883008),
  vec4(0.1416896437, 0.9899111298, 0.1752502389, 0.158113883008),
  vec4(0.9351982192, 0.3541246827, 0.1497572940, 0.158113883008),
  vec4(0.7618238114, -0.6477842854, 0.1106509834, 0.158113883008),
  vec4(0.5223395608, 0.8527375817, 0.0945550314, 0.158113883008),
  vec4(0.9989674974, -0.0454305962, 0.0698637571, 0.158113883008),
  vec4(-0.1094144198, 0.9939962197, 0.0516201463, 0.158113883008),
  vec4(0.8174767766, 0.5759615610, 0.0441111719, 0.158113883008),
  vec4(0.8991948659, -0.4375483894, 0.0325923661, 0.158113883008));
const float WATER_DETAIL_PHASES[WATER_DETAIL_COUNT] = float[WATER_DETAIL_COUNT](
  3.8832220775, 1.4832588477, 5.3664809252,
  2.9665176954, 0.5665544657, 4.4497765432,
  2.0498133134, 5.9330353909, 3.5330721612,
  1.1331089314, 5.0163310089, 2.6163677792);
WaterWaveNormal waterFilteredDetailWave(vec2 p, float clock, vec4 component, float offset) {
  float k = WATER_TAU / component.z;
  vec3 shape = waterPhase(p, component.xy, k);
  float phase = shape.x + offset - sqrt(WATER_GRAVITY * k) * clock * waterWaves.z * WATER_DETAIL_CLOCK;
  float resolved = 1.0 - smoothstep(WATER_NORMAL_FILTER_START, WATER_NORMAL_FILTER_END, fwidth(phase));
  vec2 gradient = shape.yz / k;
  vec2 slope = gradient * (cos(phase) * component.w * resolved);
  float energy = WATER_SLOPE_MEAN_SQUARE * component.w * component.w * dot(gradient, gradient);
  return WaterWaveNormal(slope, energy * (1.0 - resolved * resolved));
}
WaterWaveNormal waterFilteredDetailShape(vec2 p, float clock) {
  WaterWaveNormal detail = WaterWaveNormal(vec2(0.0), 0.0);
  for (int index = 0; index < WATER_DETAIL_COUNT; ++index)
    detail = waterAddNormals(detail, waterFilteredDetailWave(p, clock, WATER_DETAIL_COMPONENTS[index], WATER_DETAIL_PHASES[index]));
  return detail;
}
WaterWaveNormal waterFilteredShape(vec2 p, float clock) {
  WaterWaveNormal result = WaterWaveNormal(vec2(0.0), 0.0);
  for (int i = 0; i < WATER_WAVE_COUNT; ++i)
    result = waterAddNormals(result, waterFilteredWave(p, clock, WATER_COMPONENTS[i], WATER_PHASES[i]));
  return result;
}
)GLSL";
}  // namespace redgem::shaders
