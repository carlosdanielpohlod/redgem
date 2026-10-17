#pragma once

#include <string>

#include "render/shaders/copy_fade.hpp"

namespace redgem::shaders {

// Depth-only pass for the shadow map: same vertices as the lit pass (wind included), cut-out
// materials leave holes, and copies fading by distance leave the same share of their shadow out.
inline constexpr const char* kDepthHead = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
flat in vec4 fragCopy;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float alphaCutoff;
)GLSL";

inline constexpr const char* kDepthMain = R"GLSL(
void main() {
  if (fadedAway(fragCopy)) discard;
  if (colDiffuse.a * texture(texture0, fragTexCoord).a * fragColor.a < alphaCutoff) discard;
}
)GLSL";

// The whole fragment shader of the shadow map.
inline std::string depth_fragment() { return std::string(kDepthHead) + kCopyFade + kDepthMain; }

}  // namespace redgem::shaders
