#pragma once

// The vertex shader of everything a scene is made of: the lit pass and the shadow map's depth pass
// (and, in motion.hpp, where each vertex was the frame before).

#include <string>

namespace redgem::shaders {

// Every mesh is drawn instanced: each copy of it in one call finds where it is in frameTexels by
// instanceSlot, the number of its record (see instance_records.hpp), four floats a texel and
// TEXELS_PER_ROW texels to a row (defined for every shader when it is compiled, see
// shader_source.hpp). A record also gives the copy's tint and where its skeleton's bones are.
// Foliage (windAmount > 0) of a copy that sways (its record says) sways: parts higher above the
// copy's own origin move more, each copy with its own phase, from where it stands. Skinned meshes
// (skinned > 0.5) are placed by their skeleton: each vertex follows up to four bones, whose
// matrices already lead to world space. A scattered copy may fade out (and in) at the ends of the
// distances it is drawn at, and be lit as if it faced up (grass), as its record says.
//
// lit_vertex() puts its parts together: what it takes, where a copy's vertex is, and its main.
inline constexpr const char* kVertexHead = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
// Past every place raylib binds a mesh's attributes to (rlgl.h): a shader that leaves some of
// those unused (the motion pass, its normals) must not have its record number put in their place,
// where drawing the copies would take the mesh's own attribute away from the other passes.
layout(location = 9) in float instanceSlot;
uniform sampler2D frameTexels;
uniform float skinned;

uniform vec3 viewPos;               // the game camera's, in every pass: copies fade by their distance to it
uniform mat4 matView;
uniform mat4 matProjection;
uniform float windAmount;
uniform float time;
)GLSL";

// Where a copy's vertex is, read from a texture of records and bones (this frame's, or in
// motion.hpp the last frame's as well) at a time (the wind's).
inline constexpr const char* kVertexPlacement = R"GLSL(
const float WIND_SPEED = 1.3;
const float WIND_FREQUENCY = 0.35;      // phase change per meter, so neighbours are out of step
const float WIND_GUST_SPEED = 2.9;
const float WIND_GUST = 0.4;
const float WIND_HEIGHT_FULL = 6.0;     // meters above the copy's origin where the sway is strongest
const float WIND_REACH = 0.12;          // meters of sway at full height and wind 1
const float WIND_LIFT = 0.3;            // vertical share of the motion
const float SWITCH_ON = 0.5;            // switches arrive as floats: on when above this
const int TEXELS_PER_MATRIX = 4;        // a column each
const int TEXELS_PER_INSTANCE = 9;      // a record: where it is, how its normals turn, its look, how it fades
const int NORMAL_TEXEL = 4;
const int LOOK_TEXEL = 7;
const int COPY_TEXEL = 8;               // drawn from (x) and to (y) meters, faded over (z), lit as facing up (w)
const vec3 UP = vec3(0.0, 1.0, 0.0);

vec4 texelOf(sampler2D texels, int index) {
  return texelFetch(texels, ivec2(index % TEXELS_PER_ROW, index / TEXELS_PER_ROW), 0);
}

mat4 matrixOf(sampler2D texels, int first) {
  return mat4(texelOf(texels, first), texelOf(texels, first + 1), texelOf(texels, first + 2), texelOf(texels, first + 3));
}

mat4 bone(sampler2D texels, int firstBone, float joint) {
  return matrixOf(texels, firstBone + int(joint) * TEXELS_PER_MATRIX);
}

mat4 skeletonPose(sampler2D texels, int firstBone) {
  return bone(texels, firstBone, vertexBoneIds.x) * vertexBoneWeights.x +
         bone(texels, firstBone, vertexBoneIds.y) * vertexBoneWeights.y +
         bone(texels, firstBone, vertexBoneIds.z) * vertexBoneWeights.z +
         bone(texels, firstBone, vertexBoneIds.w) * vertexBoneWeights.w;
}

// Whether a copy whose record says `look` is placed by its skeleton. (A copy whose skeleton could
// not be posed has no first bone: it stays as modeled.)
bool posed(vec4 look) { return skinned > SWITCH_ON && look.a >= 0.0; }

// `world` swayed by `wind` at `clock` seconds, more the higher above `ground` (the copy's origin).
vec4 swayed(vec4 world, float ground, float wind, float clock) {
  float phase = clock * WIND_SPEED + (world.x + world.z) * WIND_FREQUENCY;
  float sway = sin(phase) + WIND_GUST * sin(clock * WIND_GUST_SPEED + world.x);
  float reach = wind * WIND_REACH * clamp((world.y - ground) / WIND_HEIGHT_FULL, 0.0, 1.0);
  world.x += sway * reach;
  world.z += cos(phase) * reach;
  world.y += sway * reach * WIND_LIFT;
  return world;
}

// The vertex placed by `toWorld` (its record's matrix, or its skeleton's pose), swayed as the
// record says.
vec4 placedBy(mat4 toWorld, float ground, sampler2D texels, int record, float clock) {
  vec4 world = toWorld * vec4(vertexPosition, 1.0);
  float wind = windAmount * texelOf(texels, record + NORMAL_TEXEL).w;  // (the record's own, not its skeleton's)
  return wind > 0.0 ? swayed(world, ground, wind, clock) : world;
}

// Where the vertex of the copy whose record starts at texel `record` of `texels` is in the world.
vec4 placed(sampler2D texels, int record, float clock) {
  vec4 look = texelOf(texels, record + LOOK_TEXEL);
  mat4 toWorld = matrixOf(texels, record);
  float ground = toWorld[3].y;
  if (posed(look)) toWorld = skeletonPose(texels, int(look.a));
  return placedBy(toWorld, ground, texels, record, clock);
}

// How much of a copy `distance` meters away is kept as it fades out at the far end of its
// distances, and in at the near one (only when it has a near one).
vec2 keptOf(vec4 copy, float distance) {
  if (copy.z <= 0.0) return vec2(1.0);
  float fadingIn = copy.x > 0.0 ? clamp((distance - copy.x) / copy.z, 0.0, 1.0) : 1.0;
  return vec2(clamp((copy.y - distance) / copy.z, 0.0, 1.0), fadingIn);
}
)GLSL";

inline constexpr const char* kLitVertexMain = R"GLSL(
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
flat out vec3 fragTint;
flat out vec4 fragCopy;                  // kept as it fades out (x) and in (y), lit as facing up (z)
flat out vec3 fragUp;

void main() {
  int record = int(instanceSlot) * TEXELS_PER_INSTANCE;
  vec4 look = texelOf(frameTexels, record + LOOK_TEXEL);
  mat4 toWorld = matrixOf(frameTexels, record);
  mat3 normalToWorld = mat3(texelOf(frameTexels, record + NORMAL_TEXEL).xyz, texelOf(frameTexels, record + NORMAL_TEXEL + 1).xyz,
                            texelOf(frameTexels, record + NORMAL_TEXEL + 2).xyz);
  vec4 copy = texelOf(frameTexels, record + COPY_TEXEL);
  fragCopy = vec4(keptOf(copy, length(toWorld[3].xyz - viewPos)), copy.w, 0.0);
  fragUp = copy.w > 0.0 ? normalize(mat3(toWorld) * UP) : UP;  // (only grass asks for it)
  float ground = toWorld[3].y;
  if (posed(look)) {
    toWorld = skeletonPose(frameTexels, int(look.a));  // (worked out once, for the position and the normals)
    normalToWorld = mat3(toWorld);  // good enough for normals: they are normalized, and rigs are not sheared
  }
  vec4 world = placedBy(toWorld, ground, frameTexels, record, time);
  fragPosition = world.xyz;
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;
  fragNormal = normalize(normalToWorld * vertexNormal);
  fragTint = look.rgb;
  gl_Position = matProjection * matView * world;
}
)GLSL";

// The lit (and depth) pass's vertex shader, whole.
inline std::string lit_vertex() { return std::string(kVertexHead) + kVertexPlacement + kLitVertexMain; }

}  // namespace redgem::shaders
