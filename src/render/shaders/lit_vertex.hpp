#pragma once

// The vertex shader of everything a scene is made of: the lit pass and the shadow map's depth pass.

namespace redgem::shaders {

// Every mesh is drawn instanced: each copy of it in one call finds where it is in frameTexels by
// instanceSlot, the number of its record (see instance_records.hpp), four floats a texel and
// TEXELS_PER_ROW texels to a row (defined for every shader when it is compiled, see
// shader_source.hpp). A record also gives the copy's tint and where its skeleton's bones are.
// Foliage (windAmount > 0) sways: parts higher above the copy's own origin move more, each copy
// with its own phase, from where it stands. Skinned meshes (skinned > 0.5) are placed by their
// skeleton: each vertex follows up to four bones, whose matrices already lead to world space.
inline constexpr const char* kLitVertex = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
in float instanceSlot;
uniform sampler2D frameTexels;
uniform float skinned;

uniform mat4 matView;
uniform mat4 matProjection;
uniform float windAmount;
uniform float time;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
flat out vec3 fragTint;

const float WIND_SPEED = 1.3;
const float WIND_FREQUENCY = 0.35;      // phase change per meter, so neighbours are out of step
const float WIND_GUST_SPEED = 2.9;
const float WIND_GUST = 0.4;
const float WIND_HEIGHT_FULL = 6.0;     // meters above the copy's origin where the sway is strongest
const float WIND_REACH = 0.12;          // meters of sway at full height and wind 1
const float WIND_LIFT = 0.3;            // vertical share of the motion
const float SWITCH_ON = 0.5;            // switches arrive as floats: on when above this
const int TEXELS_PER_MATRIX = 4;        // a column each
const int TEXELS_PER_INSTANCE = 8;      // a record: where it is, how its normals turn, its look
const int NORMAL_TEXEL = 4;
const int LOOK_TEXEL = 7;

vec4 frameTexel(int index) {
  return texelFetch(frameTexels, ivec2(index % TEXELS_PER_ROW, index / TEXELS_PER_ROW), 0);
}

mat4 frameMatrix(int first) {
  return mat4(frameTexel(first), frameTexel(first + 1), frameTexel(first + 2), frameTexel(first + 3));
}

mat4 bone(int firstBone, float joint) {
  return frameMatrix(firstBone + int(joint) * TEXELS_PER_MATRIX);
}

mat4 skeletonPose(int firstBone) {
  return bone(firstBone, vertexBoneIds.x) * vertexBoneWeights.x + bone(firstBone, vertexBoneIds.y) * vertexBoneWeights.y +
         bone(firstBone, vertexBoneIds.z) * vertexBoneWeights.z + bone(firstBone, vertexBoneIds.w) * vertexBoneWeights.w;
}

void main() {
  int record = int(instanceSlot) * TEXELS_PER_INSTANCE;
  vec4 look = frameTexel(record + LOOK_TEXEL);
  mat4 toWorld = frameMatrix(record);
  mat3 normalToWorld = mat3(frameTexel(record + NORMAL_TEXEL).xyz, frameTexel(record + NORMAL_TEXEL + 1).xyz,
                            frameTexel(record + NORMAL_TEXEL + 2).xyz);
  float ground = toWorld[3].y;
  // (A copy whose skeleton could not be posed has no first bone: it stays as modeled.)
  if (skinned > SWITCH_ON && look.a >= 0.0) {
    toWorld = skeletonPose(int(look.a));
    normalToWorld = mat3(toWorld);  // good enough for normals: they are normalized, and rigs are not sheared
  }
  vec4 world = toWorld * vec4(vertexPosition, 1.0);
  if (windAmount > 0.0) {
    float phase = time * WIND_SPEED + (world.x + world.z) * WIND_FREQUENCY;
    float sway = sin(phase) + WIND_GUST * sin(time * WIND_GUST_SPEED + world.x);
    float reach = windAmount * WIND_REACH * clamp((world.y - ground) / WIND_HEIGHT_FULL, 0.0, 1.0);
    world.x += sway * reach;
    world.z += cos(phase) * reach;
    world.y += sway * reach * WIND_LIFT;
  }
  fragPosition = world.xyz;
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;
  fragNormal = normalize(normalToWorld * vertexNormal);
  fragTint = look.rgb;
  gl_Position = matProjection * matView * world;
}
)GLSL";

}  // namespace redgem::shaders
