#pragma once

// The vertex shader of everything a scene is made of: the lit pass and the shadow map's depth pass.

namespace redgem::shaders {

// Foliage (windAmount > 0) sways: higher parts move more, each tree with its own phase.
// Skinned meshes (skinned > 0.5) are placed by their skeleton: each vertex follows up to four
// bones, whose matrices already lead to world space. MAX_BONES is gpu_profile::kMaxSkinJoints, defined
// for every shader when it is compiled (see shader_source.hpp).
inline constexpr const char* kLitVertex = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
uniform mat4 boneMatrices[MAX_BONES];
uniform float skinned;

uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform float windAmount;
uniform float time;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

const float WIND_SPEED = 1.3;
const float WIND_FREQUENCY = 0.35;      // phase change per meter, so neighbours are out of step
const float WIND_GUST_SPEED = 2.9;
const float WIND_GUST = 0.4;
const float WIND_HEIGHT_FULL = 6.0;     // meters above the ground where the sway is strongest
const float WIND_REACH = 0.12;          // meters of sway at full height and wind 1
const float WIND_LIFT = 0.3;            // vertical share of the motion
const float SWITCH_ON = 0.5;            // switches arrive as floats: on when above this

void main() {
  mat4 toWorld = matModel;
  mat3 normalToWorld = mat3(matNormal);
  if (skinned > SWITCH_ON) {
    toWorld = boneMatrices[int(vertexBoneIds.x)] * vertexBoneWeights.x + boneMatrices[int(vertexBoneIds.y)] * vertexBoneWeights.y +
              boneMatrices[int(vertexBoneIds.z)] * vertexBoneWeights.z + boneMatrices[int(vertexBoneIds.w)] * vertexBoneWeights.w;
    normalToWorld = mat3(toWorld);  // good enough for normals: they are normalized, and rigs are not sheared
  }
  vec4 world = toWorld * vec4(vertexPosition, 1.0);
  if (windAmount > 0.0) {
    float phase = time * WIND_SPEED + (world.x + world.z) * WIND_FREQUENCY;
    float sway = sin(phase) + WIND_GUST * sin(time * WIND_GUST_SPEED + world.x);
    float reach = windAmount * WIND_REACH * clamp(world.y / WIND_HEIGHT_FULL, 0.0, 1.0);
    world.x += sway * reach;
    world.z += cos(phase) * reach;
    world.y += sway * reach * WIND_LIFT;
  }
  fragPosition = world.xyz;
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;
  fragNormal = normalize(normalToWorld * vertexNormal);
  gl_Position = matProjection * matView * world;
}
)GLSL";

}  // namespace redgem::shaders
