#pragma once

// A full-screen pass: raylib's default vertex shader feeds it (fragTexCoord, fragColor).

namespace redgem::shaders {

// Copies one face of a cube capture (a 90-degree view along faceForward) into the part of the
// panorama it covers; the other pixels are left as they were.
inline constexpr const char* kPanoramaFragment = R"GLSL(
#version 330
uniform sampler2D texture0;  // the face
uniform vec3 faceForward;
uniform vec3 faceRight;
uniform vec3 faceUp;
uniform vec2 targetSize;     // of the panorama, in pixels
out vec4 finalColor;
const float PI = 3.14159265;
const float TWO_PI = 6.28318531;
const float FACE_EDGE = 1.0;
const float HALF = 0.5;      // the middle of the picture; also what turns -1..1 into 0..1
void main() {
  vec2 uv = gl_FragCoord.xy / targetSize;
  float longitude = (uv.x - HALF) * TWO_PI;
  float latitude = (uv.y - HALF) * PI;
  vec3 d = vec3(cos(latitude) * sin(longitude), sin(latitude), -cos(latitude) * cos(longitude));
  float along = dot(d, faceForward);
  if (along <= 0.0) discard;
  vec2 onFace = vec2(dot(d, faceRight), dot(d, faceUp)) / along;
  if (max(abs(onFace.x), abs(onFace.y)) > FACE_EDGE) discard;
  finalColor = vec4(texture(texture0, onFace * HALF + HALF).rgb, 1.0);
}
)GLSL";

}  // namespace redgem::shaders
