#pragma once

namespace redgem::shaders {

// Depth-only pass for the shadow map: same vertices as the lit pass (wind included), cut-out
// materials leave holes, and copies fading by distance leave the same share of their shadow out.
inline constexpr const char* kDepthFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
flat in vec4 fragCopy;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float alphaCutoff;
const vec3 NOISE_WEIGHTS = vec3(0.06711056, 0.00583715, 52.9829189);  // as the lit shader's
float pixelNoise() { return fract(NOISE_WEIGHTS.z * fract(dot(gl_FragCoord.xy, NOISE_WEIGHTS.xy))); }
void main() {
  float noise = pixelNoise();
  if (noise >= fragCopy.x || noise < 1.0 - fragCopy.y) discard;
  if (colDiffuse.a * texture(texture0, fragTexCoord).a * fragColor.a < alphaCutoff) discard;
}
)GLSL";

}  // namespace redgem::shaders
