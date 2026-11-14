#pragma once

// Auto exposure on the graphics card (see auto_exposure.hpp, whose math these follow): the lit
// picture's brightness measured into a small target, then averaged and adapted into a target of a
// single texel, which the tone mapping pass reads. Full-screen passes: raylib's default vertex
// shader feeds them (fragTexCoord).

namespace redgem::shaders {

// The lit picture -> the mean log2 luminance of each cell of a small target (in red). Each texel of
// the target gathers a grid of taps across its cell of the picture.
inline constexpr const char* kLuminanceFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 cellSize;       // of a texel of the target, in the picture's texture coordinates
out vec4 finalColor;
const int TAPS = 8;          // along each side of a cell
const vec3 LUMINANCE = vec3(0.2126, 0.7152, 0.0722);  // linear Rec. 709
const float DARKEST = 1.0 / 65536.0;  // black counts as this dark: log2 of nothing has no end
const float HALF = 0.5;

void main() {
  vec2 corner = fragTexCoord - cellSize * HALF;
  float sum = 0.0;
  for (int y = 0; y < TAPS; ++y) {
    for (int x = 0; x < TAPS; ++x) {
      vec2 at = corner + (vec2(x, y) + HALF) / float(TAPS) * cellSize;
      float luminance = dot(texture(texture0, at).rgb, LUMINANCE);
      // (A broken pixel, not a number or endless, would spoil the whole average: it counts as black.)
      bool broken = isnan(luminance) || isinf(luminance);
      sum += log2(broken ? DARKEST : max(luminance, DARKEST));
    }
  }
  finalColor = vec4(sum / float(TAPS * TAPS), 0.0, 0.0, 1.0);
}
)GLSL";

// The exposure's brightness last frame (texture0, a single texel) and the measured cells -> the
// brightness it is adapted to now (in red), as render_math::target_ev and adapted_ev work it out.
inline constexpr const char* kAdaptFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform sampler2D luminanceTexture;
uniform int luminanceSide;   // texels along each side of it
uniform float minEv;
uniform float maxEv;
uniform float speedUp;       // per second, when the scene got brighter
uniform float speedDown;     // per second, when it got darker
uniform float adaptSeconds;  // since the last frame
uniform float startAnew;     // on: straight to the scene's brightness (a cut, a new scene)
out vec4 finalColor;
const float MIDDLE_GREY_LOG2 = -2.473931;  // log2(0.18)
const float SWITCH_ON = 0.5;
const vec2 MIDDLE = vec2(0.5);

void main() {
  float sum = 0.0;
  for (int y = 0; y < luminanceSide; ++y) {
    for (int x = 0; x < luminanceSide; ++x) sum += texelFetch(luminanceTexture, ivec2(x, y), 0).r;
  }
  float meanLog2 = sum / float(luminanceSide * luminanceSide);
  float target = clamp(meanLog2 - MIDDLE_GREY_LOG2, minEv, max(maxEv, minEv));
  float current = texture(texture0, MIDDLE).r;
  float speed = max(target > current ? speedUp : speedDown, 0.0);
  float adapted = current + (target - current) * (1.0 - exp(-speed * adaptSeconds));
  finalColor = vec4(startAnew > SWITCH_ON ? target : adapted, 0.0, 0.0, 1.0);
}
)GLSL";

}  // namespace redgem::shaders
