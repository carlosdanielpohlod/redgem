#pragma once

#include <string>

#include "render/shaders/copy_fade.hpp"
#include "render/shaders/lit_shadows.hpp"

namespace redgem::shaders {

// Particles: squares that face the camera, each corner given in the world (see particle_layout).
// One that takes the light (lit) is lit at its corners by the sun, the sky and the point lights,
// as a puff that is lit on about half of it whichever way the light comes from;
// the others shine with their own color, `boost` times brighter when they glow (so they bloom).
inline constexpr const char* kParticleBoost = "boost";
inline constexpr const char* kParticleTextured = "textured";
inline constexpr const char* kParticleLook = "particleLook";
inline constexpr const char* kSceneDepth = "sceneDepth";
inline constexpr const char* kParticleVertex = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
in vec2 particleLook;  // x: meters it fades over where it meets what is behind it; y: on when lit
uniform mat4 mvp;
uniform float boost;
uniform vec3 sunColor;
uniform float ambientStrength;
uniform vec3 skyTop;
uniform vec3 skyHorizon;
uniform vec3 groundColor;
uniform int pointLightCount;
uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
uniform float pointLightRanges[MAX_POINT_LIGHTS];
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragLocalLight;
out vec3 fragSun;
out vec3 fragPosition;
out float fragSoft;
const float SWITCH_ON = 0.5;           // switches arrive as floats: on when above this
const float LIT_SHARE = 0.5;           // of a direct light, what a puff takes in on average
const float SKY_ABOVE = 0.5;           // of the sky around it: from above, around and below
const float SKY_AROUND = 0.3;
const float SKY_BELOW = 0.2;
const float LIGHT_FALLOFF_BIAS = 1.0;  // as the lit shader's: meters added so lights do not blow up up close

vec3 lightAround(vec3 position) {
  vec3 sky = skyTop * SKY_ABOVE + skyHorizon * SKY_AROUND + groundColor * SKY_BELOW;
  vec3 light = sky * ambientStrength;
  for (int i = 0; i < pointLightCount; ++i) {
    float distance = length(pointLightPositions[i] - position);
    float window = clamp(1.0 - distance / pointLightRanges[i], 0.0, 1.0);
    light += pointLightColors[i] * (window * window / (distance * distance + LIGHT_FALLOFF_BIAS)) * LIT_SHARE;
  }
  return light;
}

void main() {
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;
  fragLocalLight = particleLook.y > SWITCH_ON ? lightAround(vertexPosition) : vec3(boost);
  fragSun = particleLook.y > SWITCH_ON ? sunColor * LIT_SHARE : vec3(0.0);
  fragPosition = vertexPosition;
  fragSoft = particleLook.x;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// A particle's picture, or a soft round dot without one. A soft one fades where it nears what is
// behind it (the depth of the scene's surfaces, copied before particles are drawn).
inline constexpr const char* kParticleFragmentHead = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragLocalLight;
in vec3 fragSun;
in vec3 fragPosition;
uniform vec3 sunDirection;
in float fragSoft;
uniform sampler2D texture0;  // the particle's picture, when it has one
uniform float textured;      // 1 with a picture, 0 for the soft dot
uniform sampler2D sceneDepth;
uniform float nearPlane;
uniform float farPlane;
out vec4 finalColor;
const float GAMMA = 2.2;
const float HALF = 0.5;
const float TWO_PI = 6.28318531;
const float SOFT_FROM = 0.55;  // share of the radius where the dot starts fading out
const vec2 CENTER = vec2(0.5);
const float RADIUS = 0.5;      // of the square, in texture coordinates
const float SWITCH_ON = 0.5;   // switches arrive as floats: on when above this
const float DEPTH_RANGE = 2.0; // a depth of 0..1 is -1..1 between the near and far planes

// Meters from the camera of what lies at `depth` in the depth buffer.
float distanceAt(float depth) {
  float z = depth * DEPTH_RANGE - 1.0;
  return DEPTH_RANGE * nearPlane * farPlane / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

// 1 well in front of what is behind it, down to 0 where it meets it.
float softness() {
  if (fragSoft <= 0.0) return 1.0;
  float behind = distanceAt(texelFetch(sceneDepth, ivec2(gl_FragCoord.xy), 0).r);
  return clamp((behind - distanceAt(gl_FragCoord.z)) / fragSoft, 0.0, 1.0);
}

 )GLSL";

inline constexpr const char* kParticleFragmentMain = R"GLSL(
// Only direct sunlight is occluded: ambient and point lights keep their existing contribution.
vec3 shadowedLight() {
  if (dot(fragSun, fragSun) <= 0.0) return fragLocalLight;
  vec3 toSun = normalize(-sunDirection);
  float visible = sunVisibility(fragPosition, toSun, toSun);
  return fragLocalLight + fragSun * visible;
}

void main() {
  vec4 shape;
  if (textured > SWITCH_ON) {
    shape = texture(texture0, fragTexCoord);
  } else {
    float fromCenter = length(fragTexCoord - CENTER) / RADIUS;
    shape = vec4(1.0, 1.0, 1.0, 1.0 - smoothstep(SOFT_FROM, 1.0, fromCenter));
  }
  finalColor = vec4(pow(shape.rgb * fragColor.rgb, vec3(GAMMA)) * shadowedLight(), fragColor.a * shape.a * softness());
}
)GLSL";

inline std::string particle_fragment() {
  return std::string(kParticleFragmentHead) + "\n#define SOFT_SHADOWS\n" + kCopyFade + kLitShadows +
         kParticleFragmentMain;
}

// The sun sees a sprite facing it. Alpha is cut out rather than accumulated as volume density.
inline constexpr const char* kParticleShadowVertex = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out float fragAlpha;
void main() {
  fragTexCoord = vertexTexCoord;
  fragAlpha = vertexColor.a;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

inline constexpr const char* kParticleShadowFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
in float fragAlpha;
uniform sampler2D texture0;
uniform float textured;
const float ALPHA_CUTOFF = 0.5;  // only the opaque half of a sprite writes sun depth
const float SWITCH_ON = 0.5;
const float SOFT_FROM = 0.55;    // same circular profile as the color pass
const vec2 CENTER = vec2(0.5);
const float RADIUS = 0.5;
void main() {
  float alpha = textured > SWITCH_ON ? texture(texture0, fragTexCoord).a
      : 1.0 - smoothstep(SOFT_FROM, 1.0, length(fragTexCoord - CENTER) / RADIUS);
  if (fragAlpha * alpha < ALPHA_CUTOFF) discard;
}
)GLSL";

}  // namespace redgem::shaders
