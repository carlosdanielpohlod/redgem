#pragma once

// A full-screen pass: raylib's default vertex shader feeds it (fragTexCoord, fragColor).

namespace redgem::shaders {

// FXAA (after Timothy Lottes' FXAA 3.11, console quality): smooths edges using the luma in alpha.
inline constexpr const char* kAntialiasFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 texelSize;
out vec4 finalColor;
const float EDGE_THRESHOLD = 0.125;     // local contrast needed to treat a pixel as an edge
const float EDGE_THRESHOLD_MIN = 0.0312;
const float SPAN_MAX = 8.0;             // longest blur along an edge, in texels
const float REDUCE_MUL = 0.125;
const float REDUCE_MIN = 0.0078125;
const float NEAR_TAP = 0.1666667;       // 1/3 - 1/2
const float FAR_TAP = 0.5;
const float AVERAGE_OF_TWO = 0.5;
const float AVERAGE_OF_FOUR = 0.25;
void main() {
  vec4 center = texture(texture0, fragTexCoord);
  float lumaNW = texture(texture0, fragTexCoord + vec2(-1.0, -1.0) * texelSize).a;
  float lumaNE = texture(texture0, fragTexCoord + vec2(1.0, -1.0) * texelSize).a;
  float lumaSW = texture(texture0, fragTexCoord + vec2(-1.0, 1.0) * texelSize).a;
  float lumaSE = texture(texture0, fragTexCoord + vec2(1.0, 1.0) * texelSize).a;
  float lumaMin = min(center.a, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
  float lumaMax = max(center.a, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
  if (lumaMax - lumaMin < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD)) {
    finalColor = vec4(center.rgb, 1.0);
    return;
  }
  vec2 dir = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), (lumaNW + lumaSW) - (lumaNE + lumaSE));
  float reduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * AVERAGE_OF_FOUR * REDUCE_MUL, REDUCE_MIN);
  float scale = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
  dir = clamp(dir * scale, vec2(-SPAN_MAX), vec2(SPAN_MAX)) * texelSize;
  vec4 near = AVERAGE_OF_TWO * (texture(texture0, fragTexCoord + dir * -NEAR_TAP) + texture(texture0, fragTexCoord + dir * NEAR_TAP));
  vec4 far = AVERAGE_OF_TWO * near + AVERAGE_OF_FOUR * (texture(texture0, fragTexCoord + dir * -FAR_TAP) + texture(texture0, fragTexCoord + dir * FAR_TAP));
  finalColor = vec4((far.a < lumaMin || far.a > lumaMax) ? near.rgb : far.rgb, 1.0);
}
)GLSL";

}  // namespace redgem::shaders
