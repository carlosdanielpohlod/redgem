#pragma once

namespace redgem::shaders {

// Decals: marks on the scene's surfaces, their corners given in the world. A decal darkens what is
// already drawn under it (it is mixed in by multiplying), so it takes that surface's light and
// shade: what comes out is the color to multiply by, white where the mark is not.
inline constexpr const char* kDecalVertex = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;

void main() {
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

inline constexpr const char* kDecalFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
out vec4 finalColor;
const vec3 UNMARKED = vec3(1.0);  // multiplies what is under it by nothing

void main() {
  vec4 mark = texture(texture0, fragTexCoord) * fragColor;
  finalColor = vec4(mix(UNMARKED, mark.rgb, mark.a), 1.0);
}
)GLSL";

}  // namespace redgem::shaders
