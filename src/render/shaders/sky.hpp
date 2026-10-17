#pragma once

namespace redgem::shaders {

// The sky dome: drawn around the camera before everything else.
inline constexpr const char* kSkyVertex = R"GLSL(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
out vec3 direction;
void main() {
  direction = vertexPosition;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// A gradient with the sun's disc in it, or a picture of the sky (its own sun included), laid as
// render/sky_directions.hpp says.
inline constexpr const char* kSkyFragment = R"GLSL(
#version 330
in vec3 direction;
uniform sampler2D texture0;  // the sky's picture, when it has one
uniform float skyLight;      // 1: the sky is that picture
uniform vec2 skyTurn;        // cosine and sine of the turn that leads from the world into the picture
uniform float skyIntensity;
uniform vec3 skyTop;
uniform vec3 skyHorizon;
uniform vec3 groundColor;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunGlow;
out vec4 finalColor;

const float ZENITH_CURVE = 0.6;    // the same curve the lit shader uses for ambient light
const float GROUND_BLEND = 0.6;
const float GROUND_TINT = 0.25;
const float DISC_SHARPNESS = 1500.0;   // the sun's disc
const float DISC_BRIGHTNESS = 12.0;
const float HALO_SHARPNESS = 24.0;     // the glow around it
const float HALO_BRIGHTNESS = 0.9;
const float HAZE_SHARPNESS = 3.0;      // the warm side of the sky
const float HAZE_BRIGHTNESS = 0.25;
const float EPSILON = 0.0001;
const float SWITCH_ON = 0.5;
const float HALF = 0.5;
const float PI = 3.14159265;
const float TWO_PI = 6.28318531;

vec3 gradient(vec3 d) {
  vec3 color = mix(skyHorizon, skyTop, pow(clamp(d.y, 0.0, 1.0), ZENITH_CURVE));
  // Below the horizon the dome keeps the horizon color, slightly tinted by the ground far down.
  color = mix(color, mix(skyHorizon, groundColor, GROUND_TINT), smoothstep(0.0, -GROUND_BLEND, d.y));
  float toSun = max(dot(d, normalize(-sunDirection)), 0.0);
  vec3 tint = sunColor / (max(sunColor.r, max(sunColor.g, sunColor.b)) + EPSILON);
  return color + tint * sunGlow * (pow(toSun, DISC_SHARPNESS) * DISC_BRIGHTNESS + pow(toSun, HALO_SHARPNESS) * HALO_BRIGHTNESS +
                                   pow(toSun, HAZE_SHARPNESS) * HAZE_BRIGHTNESS);
}

vec3 picture(vec3 d) {
  vec3 p = vec3(d.x * skyTurn.x - d.z * skyTurn.y, d.y, d.x * skyTurn.y + d.z * skyTurn.x);
  vec2 uv = vec2(HALF + atan(p.z, p.x) / TWO_PI, HALF - asin(clamp(p.y, -1.0, 1.0)) / PI);
  return texture(texture0, uv).rgb * skyIntensity;
}

void main() {
  vec3 d = normalize(direction);
  finalColor = vec4(skyLight > SWITCH_ON ? picture(d) : gradient(d), 1.0);
}
)GLSL";

}  // namespace redgem::shaders
