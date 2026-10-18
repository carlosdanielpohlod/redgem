#pragma once

// Where each pixel of the scene's surfaces was the frame before, for temporal anti-aliasing (see
// temporal.hpp): the opaque surfaces drawn again, each vertex placed twice, as now and as the last
// frame's records, bones and wind placed it (previousTexels, previousTime), and seen through the
// last frame's camera. Neither view is moved by the frame's jitter: what moved is only what moved
// in the world. The picture is drawn where the lit pass draws it, jitter and all.

#include <string>

#include "render/shaders/copy_fade.hpp"
#include "render/shaders/lit_vertex.hpp"

namespace redgem::shaders {

inline constexpr const char* kPreviousTexels = "previousTexels";
inline constexpr const char* kPreviousTime = "previousTime";
inline constexpr const char* kViewProjection = "viewProjection";
inline constexpr const char* kCovering = "covering";
inline constexpr const char* kPreviousViewProjection = "previousViewProjection";

inline constexpr const char* kMotionVertexMain = R"GLSL(
uniform sampler2D previousTexels;
uniform float previousTime;
uniform mat4 viewProjection;           // this frame's camera, without the jitter
uniform mat4 previousViewProjection;   // the last frame's
out vec2 fragTexCoord;
out vec4 fragColor;
flat out vec4 fragCopy;
out vec4 nowClip;
out vec4 beforeClip;

void main() {
  int record = int(instanceSlot) * TEXELS_PER_INSTANCE;
  mat4 toWorld = matrixOf(frameTexels, record);
  fragCopy = vec4(keptOf(texelOf(frameTexels, record + COPY_TEXEL), length(toWorld[3].xyz - viewPos)), 0.0, 0.0);
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;
  vec4 world = placed(frameTexels, record, time);
  nowClip = viewProjection * world;
  beforeClip = previousViewProjection * placed(previousTexels, record, previousTime);
  gl_Position = matProjection * matView * world;
}
)GLSL";

// What moved, in texture coordinates of the picture (rg), and that a surface was drawn there (b).
// Cut-out and fading surfaces leave out what the lit pass leaves out (alpha tested, not covered).
// Covering, a see-through surface writes how much of the pixel it covers instead, into alpha.
inline constexpr const char* kMotionHead = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
flat in vec4 fragCopy;
in vec4 nowClip;
in vec4 beforeClip;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float alphaCutoff;
uniform float alphaDithered;
uniform float alphaCeiling;
uniform float covering;  // on: how much a see-through surface covers the pixel, into alpha alone
out vec4 finalColor;
)GLSL";

inline constexpr const char* kMotionMain = R"GLSL(
const float SWITCH_ON = 0.5;
const float NDC_TO_TEXTURE = 0.5;   // normalized device coordinates span 2 across, a texture 1
const float NOWHERE = 0.0001;       // a w this small was behind the last frame's camera
const float DRAWN = 1.0;
bool leftOut(float alpha) {
  if (alpha >= alphaCeiling) return true;
  return alphaDithered > SWITCH_ON ? alpha <= pixelNoise() : alpha < alphaCutoff;
}
void main() {
  if (fadedAway(fragCopy)) discard;
  float alpha = colDiffuse.a * texture(texture0, fragTexCoord).a * fragColor.a;
  if (leftOut(alpha)) discard;
  if (covering > SWITCH_ON) {
    finalColor = vec4(0.0, 0.0, 0.0, alpha);
    return;
  }
  vec2 now = nowClip.xy / nowClip.w;
  vec2 before = beforeClip.w > NOWHERE ? beforeClip.xy / beforeClip.w : now;
  finalColor = vec4((now - before) * NDC_TO_TEXTURE, DRAWN, 0.0);
}
)GLSL";

// The motion pass's fragment shader, whole: its fading copies as the lit pass's (copy_fade.hpp).
inline std::string motion_fragment() { return std::string(kMotionHead) + kCopyFade + kMotionMain; }

// The motion pass's vertex shader, whole: the lit pass's placement, done twice.
inline std::string motion_vertex() { return std::string(kVertexHead) + kVertexPlacement + kMotionVertexMain; }

}  // namespace redgem::shaders
