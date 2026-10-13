#pragma once

// A full-screen pass: raylib's default vertex shader feeds it (fragTexCoord, fragColor).

namespace redgem::shaders {

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
uniform float contrast;      // 1 = unchanged
uniform float saturation;    // 1 = unchanged
uniform float grain;         // film grain strength
uniform float aberration;    // color fringing toward the corners
uniform float motionBlur;    // 0..1: streaks away from the center, as when moving fast
uniform float time;
out vec4 finalColor;
const float GAMMA = 2.2;
const int BLUR_TAPS = 10;
const float BLUR_REACH = 0.07;        // of the way to the center, at full motion blur
const float BLUR_CLEAR_RADIUS = 0.3;  // the middle of the picture stays sharp
const float ABERRATION_REACH = 0.0035;
const float MID_GREY = 0.5;
const float GRAIN_SCALE = 0.08;
const vec2 NOISE_DRIFT = vec2(173.0, 311.0);  // pixels the noise pattern jumps each frame, so it never sits still
const float ACES_A = 2.51;
const float ACES_B = 0.03;
const float ACES_C = 2.43;
const float ACES_D = 0.59;
const float ACES_E = 0.14;
const float VIGNETTE_START = 0.35;  // distance from the center where the darkening begins
const float VIGNETTE_END = 1.0;
const vec3 LUMA = vec3(0.299, 0.587, 0.114);
const vec2 MIDDLE = vec2(0.5);        // of the picture, in texture coordinates
const float EDGE_DISTANCE = 2.0;      // distances from the middle are counted so that the edges are at 1
const float NOISE_MIDDLE = 0.5;       // noise is 0..1: around this it darkens as often as it lightens
const vec2 NOISE_DIRECTION = vec2(12.9898, 78.233);  // the usual one-line hash: a sine too fast to follow
const float NOISE_SCALE = 43758.5453;
float noise(vec2 at) { return fract(sin(dot(at, NOISE_DIRECTION)) * NOISE_SCALE); }

// The lit scene at a point, with the lens's color fringes: red and blue land a little apart.
vec3 lens(vec2 at, vec2 fromMiddle) {
  vec2 fringe = fromMiddle * aberration * ABERRATION_REACH;
  return vec3(texture(texture0, at + fringe).r, texture(texture0, at).g, texture(texture0, at - fringe).b);
}

void main() {
  vec2 fromMiddle = fragTexCoord - MIDDLE;
  vec3 scene = lens(fragTexCoord, fromMiddle);
  if (motionBlur > 0.0) {
    // Speed: everything streaks away from where the camera is heading, more toward the edges.
    float streak = motionBlur * BLUR_REACH * smoothstep(BLUR_CLEAR_RADIUS, 1.0, length(fromMiddle) * EDGE_DISTANCE);
    // Each pixel starts its taps at a slightly different place: the steps between them then show
    // as fine noise instead of bands.
    float start = noise(gl_FragCoord.xy + fract(time) * NOISE_DRIFT);
    for (int tap = 1; tap < BLUR_TAPS; tap++) {
      scene += lens(fragTexCoord - fromMiddle * streak * ((float(tap) - start) / float(BLUR_TAPS - 1)), fromMiddle);
    }
    scene /= float(BLUR_TAPS);
  }
  scene *= mix(1.0, texture(occlusionTexture, fragTexCoord).r, occlusionAmount);
  vec3 color = (scene + texture(bloomTexture, fragTexCoord).rgb * bloomStrength) * exposure;
  color = clamp((color * (ACES_A * color + ACES_B)) / (color * (ACES_C * color + ACES_D) + ACES_E), 0.0, 1.0);
  float fromCenter = length(fragTexCoord - MIDDLE) * EDGE_DISTANCE;
  color *= 1.0 - vignette * smoothstep(VIGNETTE_START, VIGNETTE_END + VIGNETTE_START, fromCenter);
  color = pow(color, vec3(1.0 / GAMMA));
  // The grade: on the picture as the eye sees it.
  color = mix(vec3(dot(color, LUMA)), color, saturation);
  color = (color - MID_GREY) * contrast + MID_GREY;
  color += (noise(gl_FragCoord.xy + fract(time) * NOISE_DRIFT) - NOISE_MIDDLE) * grain * GRAIN_SCALE;
  color = clamp(color, 0.0, 1.0);
  finalColor = vec4(color, dot(color, LUMA));
}
)GLSL";

}  // namespace redgem::shaders
