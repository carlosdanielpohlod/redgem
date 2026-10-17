#pragma once

namespace redgem::shaders {

// What glowing particles gathered on their own add to the picture: however many of them lie over
// each other, a pixel's brightest channel comes near `glowLimit` (linear light) and never past it,
// smoothly, and the others keep their share of it, so that a crowd of flames stays an orange fire
// instead of a white haze. Faint glow is added nearly as it is.
inline constexpr const char* kGlowLimit = "glowLimit";
inline constexpr const char* kGlowLimitFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;  // the glow gathered
uniform float glowLimit;
out vec4 finalColor;
void main() {
  vec3 glow = texture(texture0, fragTexCoord).rgb;
  float brightest = max(max(glow.r, glow.g), glow.b);
  float kept = brightest > 0.0 ? glowLimit * (1.0 - exp(-brightest / glowLimit)) / brightest : 1.0;
  finalColor = vec4(glow * kept, 0.0);
}
)GLSL";

}  // namespace redgem::shaders
