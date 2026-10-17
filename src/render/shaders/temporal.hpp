#pragma once

// Temporal anti-aliasing's full-screen passes (see temporal.hpp): the frame mixed with the ones
// before it, and the result sharpened a little. raylib's default vertex shader feeds them.

namespace redgem::shaders {

inline constexpr const char* kHistoryTexture = "historyTexture";
inline constexpr const char* kMotionTexture = "motionTexture";
inline constexpr const char* kMotionDepth = "motionDepth";
inline constexpr const char* kReprojection = "reprojection";
inline constexpr const char* kHistoryValid = "historyValid";
inline constexpr const char* kSharpness = "sharpness";

// The lit picture (texture0, linear light, its view jittered) mixed with the frames before it
// (historyTexture), read where each pixel was then: what the motion pass drew there, or for what
// it did not draw (the sky) where the camera's move takes its depth. The frames before are first
// held to the colors around the pixel now, in YCoCg (variance clipping): what is no longer there
// (an object moved away, a flash faded, the camera cut) is not smeared over the picture. Where
// particles cover the pixel (the motion pass's alpha) the frames before count for less: particles
// write no motion. Bright colors are compressed while they are mixed, so that a single bright
// pixel does not flicker on and off.
inline constexpr const char* kTemporalFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform sampler2D historyTexture;
uniform sampler2D motionTexture;   // rg: motion in texture coordinates, b: a surface drew it, a: particles' cover
uniform sampler2D motionDepth;
uniform mat4 reprojection;         // this frame's normalized device coordinates to the last frame's clip ones
uniform vec2 texelSize;
uniform float historyValid;
out vec4 finalColor;
const float HISTORY_WEIGHT = 0.9;            // of the frames before, where nothing changed
const float COVERED_HISTORY_WEIGHT = 0.5;    // where particles cover the pixel
const float MOVING_HISTORY_WEIGHT = 0.6;     // where the pixel moves FAST_PIXELS or more a frame
const float FAST_PIXELS = 16.0;
const float SPLIT_PIXELS = 2.0;              // the pixel's motion and its nearest neighbour's this far apart: an edge opening
const float SPLIT_HISTORY_WEIGHT = 0.5;      // where an edge opens (what was behind is newly seen)
// Standard deviations the frames before may stray from the colors around the pixel: far where it
// stands still (a thin blade, missed by some of the jittered frames, still adds up), near once it
// moves CLIP_PIXELS a frame (what moved away leaves no trail).
const float STILL_SPREAD = 1.5;
const float MOVING_SPREAD = 1.0;
const float CLIP_PIXELS = 2.0;
const float SWITCH_ON = 0.5;
const float DEPTH_RANGE = 2.0;               // a depth of 0..1 is -1..1 in normalized device coordinates
const float NDC_TO_TEXTURE = 0.5;
const float NOWHERE = 0.0001;
const float NEIGHBOURS = 9.0;                // the pixel and the eight around it
const float FARTHEST = 1.0;
const float MOST_COMPRESSED = 0.999;         // a compressed color this bright is about a thousand times white
const vec3 LUMA = vec3(0.25, 0.5, 0.25);     // YCoCg's rows
const vec3 ORANGE = vec3(0.5, 0.0, -0.5);
const vec3 GREEN = vec3(-0.25, 0.5, -0.25);

vec3 toYCoCg(vec3 c) { return vec3(dot(c, LUMA), dot(c, ORANGE), dot(c, GREEN)); }
vec3 fromYCoCg(vec3 c) { return vec3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }
vec3 compress(vec3 c) { return c / (1.0 + max(c.r, max(c.g, c.b))); }
vec3 expand(vec3 c) { return c / max(1.0 - max(c.r, max(c.g, c.b)), NOWHERE); }
// `uv` held inside the picture's outermost texels: nothing is read past its edges.
vec2 inside(vec2 uv) { return clamp(uv, texelSize * 0.5, 1.0 - texelSize * 0.5); }
vec3 colorAt(vec2 uv) { return toYCoCg(compress(texture(texture0, inside(uv)).rgb)); }


// How far what lies at `depth` at `uv` moved on the picture because the camera did.
vec2 cameraMotion(vec2 uv, float depth) {
  vec4 now = vec4(uv / NDC_TO_TEXTURE - 1.0, depth * DEPTH_RANGE - 1.0, 1.0);
  vec4 before = reprojection * now;
  return before.w > NOWHERE ? (now.xy - before.xy / before.w) * NDC_TO_TEXTURE : vec2(0.0);
}

// What the motion pass drew at `uv`, or where nothing was drawn (the sky) the camera's motion.
vec2 drawnMotion(vec2 uv, float depth) {
  vec4 motion = texture(motionTexture, uv);
  return motion.b > SWITCH_ON ? motion.rg : cameraMotion(uv, depth);
}

// The motion of the nearest surface around the pixel (edges move with what is in front), and in
// `split` how far, in pixels, the pixel's own motion is from it: an edge that opens.
vec2 motionAt(vec2 uv, out float split) {
  vec2 nearest = uv;
  float depth = FARTHEST + 1.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      vec2 at = inside(uv + vec2(x, y) * texelSize);
      float d = texture(motionDepth, at).r;
      if (d < depth) { depth = d; nearest = at; }
    }
  }
  vec2 motion = drawnMotion(nearest, depth);
  split = length((drawnMotion(uv, texture(motionDepth, uv).r) - motion) / texelSize);
  return motion;
}

// The frames before at `uv`, filtered by a Catmull-Rom spline in five reads (bilinear ones,
// weighted): sharper than a bilinear read, which blurs a little more every frame.
vec3 historyAt(vec2 uv) {
  vec2 position = uv / texelSize;
  vec2 center = floor(position - 0.5) + 0.5;
  vec2 f = position - center;
  vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
  vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
  vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
  vec2 w3 = f * f * (-0.5 + 0.5 * f);
  vec2 w12 = w1 + w2;
  vec2 at0 = (center - 1.0) * texelSize;
  vec2 at3 = (center + 2.0) * texelSize;
  vec2 at12 = (center + w2 / w12) * texelSize;
  vec3 sum = texture(historyTexture, vec2(at12.x, at0.y)).rgb * (w12.x * w0.y) +
             texture(historyTexture, vec2(at0.x, at12.y)).rgb * (w0.x * w12.y) +
             texture(historyTexture, at12).rgb * (w12.x * w12.y) +
             texture(historyTexture, vec2(at3.x, at12.y)).rgb * (w3.x * w12.y) +
             texture(historyTexture, vec2(at12.x, at3.y)).rgb * (w12.x * w3.y);
  float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
  return max(sum / weight, vec3(0.0));
}

// `color` pulled toward the middle of the box from `low` to `high` until it is inside it.
vec3 clipToBox(vec3 color, vec3 low, vec3 high) {
  vec3 middle = (low + high) * 0.5;
  vec3 extent = (high - low) * 0.5 + NOWHERE;
  vec3 away = color - middle;
  vec3 units = abs(away / extent);
  float most = max(units.x, max(units.y, units.z));
  return most > 1.0 ? middle + away / most : color;
}

// The frames before at `uv`, held to the colors around the pixel now: within some standard
// deviations of their mean (see STILL_SPREAD), and, the more it moves, the less past the darkest and
// brightest of them.
vec3 heldHistory(vec2 uv, float speed) {
  vec3 sum = vec3(0.0);
  vec3 squares = vec3(0.0);
  vec3 low = vec3(FARTHEST);
  vec3 high = vec3(-FARTHEST);
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      vec3 c = colorAt(fragTexCoord + vec2(x, y) * texelSize);
      sum += c;
      squares += c * c;
      low = min(low, c);
      high = max(high, c);
    }
  }
  vec3 mean = sum / NEIGHBOURS;
  float moving = clamp(speed / CLIP_PIXELS, 0.0, 1.0);
  vec3 spread = sqrt(max(squares / NEIGHBOURS - mean * mean, 0.0)) * mix(STILL_SPREAD, MOVING_SPREAD, moving);
  vec3 history = toYCoCg(compress(historyAt(uv)));
  vec3 held = clipToBox(history, mean - spread, mean + spread);
  return mix(held, clipToBox(held, low, high), moving);
}

// A history that went wrong on some graphics card (not a number, or endless) is not mixed in: it
// would spread from frame to frame.
bool broken(vec3 color) { return any(isnan(color)) || any(isinf(color)); }

// How much the frames before count: less where particles cover the pixel, where it moves fast
// (the frames before are then blurrier, and fewer of them are worth anything at a low frame
// rate), and where an edge opens.
float historyWeight(vec2 motion, float split) {
  float covered = clamp(texture(motionTexture, fragTexCoord).a, 0.0, 1.0);
  float speed = clamp(length(motion / texelSize) / FAST_PIXELS, 0.0, 1.0);
  float weight = min(mix(HISTORY_WEIGHT, COVERED_HISTORY_WEIGHT, covered), mix(HISTORY_WEIGHT, MOVING_HISTORY_WEIGHT, speed));
  return split > SPLIT_PIXELS ? min(weight, SPLIT_HISTORY_WEIGHT) : weight;
}

bool outside(vec2 uv) { return any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))); }

void main() {
  vec4 current = texture(texture0, fragTexCoord);
  float split;
  vec2 motion = motionAt(fragTexCoord, split);
  vec2 before = fragTexCoord - motion;
  if (historyValid < SWITCH_ON || outside(before)) {
    finalColor = current;
    return;
  }
  float weight = historyWeight(motion, split);
  vec3 held = heldHistory(before, length(motion / texelSize));
  vec3 mixed = broken(held) ? toYCoCg(compress(current.rgb)) : mix(toYCoCg(compress(current.rgb)), held, weight);
  finalColor = vec4(expand(clamp(fromYCoCg(mixed), 0.0, MOST_COMPRESSED)), current.a);
}
)GLSL";

// After the frames are mixed, the tone mapped picture sharpened a little: each pixel pushed away
// from the average of the four next to it, by `sharpness`, no farther than the colors around it.
inline constexpr const char* kSharpenFragment = R"GLSL(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 texelSize;
uniform float sharpness;
out vec4 finalColor;
const float AVERAGE_OF_FOUR = 0.25;
void main() {
  vec3 center = texture(texture0, fragTexCoord).rgb;
  vec3 left = texture(texture0, fragTexCoord - vec2(texelSize.x, 0.0)).rgb;
  vec3 right = texture(texture0, fragTexCoord + vec2(texelSize.x, 0.0)).rgb;
  vec3 down = texture(texture0, fragTexCoord - vec2(0.0, texelSize.y)).rgb;
  vec3 up = texture(texture0, fragTexCoord + vec2(0.0, texelSize.y)).rgb;
  vec3 low = min(center, min(min(left, right), min(down, up)));
  vec3 high = max(center, max(max(left, right), max(down, up)));
  vec3 sharpened = center + (center - (left + right + down + up) * AVERAGE_OF_FOUR) * sharpness;
  finalColor = vec4(clamp(sharpened, low, high), 1.0);
}
)GLSL";

}  // namespace redgem::shaders
