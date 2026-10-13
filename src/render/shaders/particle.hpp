#pragma once

namespace redgem::shaders {

// A particle: a soft round dot, drawn on a square that faces the camera. `boost` makes glowing ones
// brighter than white, so they bloom.
inline constexpr const char* kParticleBoost = "boost";
inline constexpr const char* kParticleTextured = "textured";
inline constexpr const char* kParticleFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;  // the particle's picture, when it has one
uniform float textured;      // 1 with a picture, 0 for the soft dot
uniform float boost;
out vec4 finalColor;
const float GAMMA = 2.2;
const float SOFT_FROM = 0.55;  // share of the radius where the dot starts fading out
const vec2 CENTER = vec2(0.5);
const float RADIUS = 0.5;      // of the square, in texture coordinates
const float SWITCH_ON = 0.5;   // switches arrive as floats: on when above this
void main() {
  vec4 shape;
  if (textured > SWITCH_ON) {
    shape = texture(texture0, fragTexCoord);
  } else {
    float fromCenter = length(fragTexCoord - CENTER) / RADIUS;
    shape = vec4(1.0, 1.0, 1.0, 1.0 - smoothstep(SOFT_FROM, 1.0, fromCenter));
  }
  finalColor = vec4(pow(shape.rgb * fragColor.rgb, vec3(GAMMA)) * boost, fragColor.a * shape.a);
}
)GLSL";

}  // namespace redgem::shaders
