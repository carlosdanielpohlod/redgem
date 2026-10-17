#pragma once

namespace redgem::shaders {

// Particles: squares that face the camera, each corner given in the world (see particle_layout).
// One that takes the light (lit) is lit at its corners by the sun, the sky and the point lights,
// as a puff that is lit on about half of it whichever way the light comes from (no shadows);
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
out vec3 fragLight;
out float fragSoft;
const float SWITCH_ON = 0.5;           // switches arrive as floats: on when above this
const float LIT_SHARE = 0.5;           // of a direct light, what a puff takes in on average
const float SKY_ABOVE = 0.5;           // of the sky around it: from above, around and below
const float SKY_AROUND = 0.3;
const float SKY_BELOW = 0.2;
const float LIGHT_FALLOFF_BIAS = 1.0;  // as the lit shader's: meters added so lights do not blow up up close

vec3 lightAround(vec3 position) {
  vec3 sky = skyTop * SKY_ABOVE + skyHorizon * SKY_AROUND + groundColor * SKY_BELOW;
  vec3 light = sunColor * LIT_SHARE + sky * ambientStrength;
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
  fragLight = particleLook.y > SWITCH_ON ? lightAround(vertexPosition) : vec3(boost);
  fragSoft = particleLook.x;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// A particle's picture, or a soft round dot without one. A soft one fades where it nears what is
// behind it (the depth of the scene's surfaces, copied before particles are drawn).
inline constexpr const char* kParticleFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragLight;
in float fragSoft;
uniform sampler2D texture0;  // the particle's picture, when it has one
uniform float textured;      // 1 with a picture, 0 for the soft dot
uniform sampler2D sceneDepth;
uniform float nearPlane;
uniform float farPlane;
out vec4 finalColor;
const float GAMMA = 2.2;
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

void main() {
  vec4 shape;
  if (textured > SWITCH_ON) {
    shape = texture(texture0, fragTexCoord);
  } else {
    float fromCenter = length(fragTexCoord - CENTER) / RADIUS;
    shape = vec4(1.0, 1.0, 1.0, 1.0 - smoothstep(SOFT_FROM, 1.0, fromCenter));
  }
  finalColor = vec4(pow(shape.rgb * fragColor.rgb, vec3(GAMMA)) * fragLight, fragColor.a * shape.a * softness());
}
)GLSL";

}  // namespace redgem::shaders
