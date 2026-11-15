#pragma once

// Bloom's two full-screen passes (raylib's default vertex shader feeds them: fragTexCoord,
// fragColor). The blur also smooths the ambient occlusion.

namespace redgem::shaders {

// Keeps what is brighter than the threshold (drawn into a smaller target: the bloom source), as the
// picture will be exposed (auto exposure's adaptedScale, which with_adapted_scale puts in): what
// glows is what shows bright, however dark the scene the eye adapted to.
inline constexpr const char* kBrightFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float threshold;
out vec4 finalColor;
const float KNEE = 0.6;         // soft start above the threshold
const float MAX_BRIGHT = 8.0;   // tiny mirror-like highlights must not make the glow flicker
void main() {
  vec3 color = texture(texture0, fragTexCoord).rgb;
  // A broken pixel (not a number) must not be smeared over the screen by the blur.
  if (any(isnan(color)) || any(isinf(color))) color = vec3(0.0);
  float scale = adaptedScale();
  vec3 exposed = min(color * scale, vec3(MAX_BRIGHT));
  float brightest = max(exposed.r, max(exposed.g, exposed.b));
  finalColor = vec4(exposed / scale * smoothstep(threshold, threshold + KNEE, brightest), 1.0);
}
)GLSL";

// One direction of a separable Gaussian blur (9 taps using bilinear filtering).
inline constexpr const char* kBlurFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 blurStep;  // one texel along the blur direction
out vec4 finalColor;
const float OFFSET_1 = 1.3846153846;
const float OFFSET_2 = 3.2307692308;
const float WEIGHT_0 = 0.2270270270;
const float WEIGHT_1 = 0.3162162162;
const float WEIGHT_2 = 0.0702702703;
void main() {
  vec3 color = texture(texture0, fragTexCoord).rgb * WEIGHT_0;
  color += texture(texture0, fragTexCoord + blurStep * OFFSET_1).rgb * WEIGHT_1;
  color += texture(texture0, fragTexCoord - blurStep * OFFSET_1).rgb * WEIGHT_1;
  color += texture(texture0, fragTexCoord + blurStep * OFFSET_2).rgb * WEIGHT_2;
  color += texture(texture0, fragTexCoord - blurStep * OFFSET_2).rgb * WEIGHT_2;
  finalColor = vec4(color, 1.0);
}
)GLSL";

}  // namespace redgem::shaders
