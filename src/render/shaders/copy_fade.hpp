#pragma once

// Copies fading out (or in) by distance, the same in the lit pass, the shadow map's and the motion
// pass's: the
// noise that picks which of their pixels go, and whether a pixel goes. A part of a fragment
// shader, put after its version line.

namespace redgem::shaders {

inline constexpr const char* kCopyFade = R"GLSL(
const vec3 NOISE_WEIGHTS = vec3(0.06711056, 0.00583715, 52.9829189);  // interleaved gradient noise

uniform float noiseShift;  // 0..1: how the pattern is shifted this frame (0 unless set)

// A number 0..1 that changes from pixel to pixel without a pattern the eye picks up (and, shifted
// by noiseShift, from frame to frame: see temporal.hpp).
float pixelNoise() { return fract(fract(NOISE_WEIGHTS.z * fract(dot(gl_FragCoord.xy, NOISE_WEIGHTS.xy))) + noiseShift); }

// Whether a pixel of a copy that is fading out (or in) by distance is left out: a share of the
// pixels that grows as it fades. A copy fading in leaves out exactly the pixels that one fading
// out over the same distances keeps, so two levels of detail cross over without a gap.
bool fadedAway(vec4 copy) {
  if (min(copy.x, copy.y) >= 1.0) return false;  // (most copies: no noise worked out)
  float noise = pixelNoise();
  return noise >= copy.x || noise < 1.0 - copy.y;
}
)GLSL";

}  // namespace redgem::shaders
