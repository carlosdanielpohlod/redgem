#pragma once

namespace redgem::shaders {

// Depth-only pass for the shadow map: same vertices as the lit pass (wind included), and cut-out
// materials leave holes.
inline constexpr const char* kDepthFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float alphaCutoff;
void main() {
  if (colDiffuse.a * texture(texture0, fragTexCoord).a * fragColor.a < alphaCutoff) discard;
}
)GLSL";

}  // namespace redgem::shaders
