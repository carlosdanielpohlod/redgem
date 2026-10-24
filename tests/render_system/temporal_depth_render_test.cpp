// Isolated temporal resolve inputs distinguish depth rejection from color clipping and velocity rejection.
#include <doctest/doctest.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <array>
#include <cmath>
#include <string>
#include <optional>

#include "render/shader_source.hpp"
#include "render/shaders/temporal.hpp"
#include "render_system/render_stage.hpp"

namespace {

constexpr int kDepthSide = 8;  // enough neighbors for the resolve's variance clipping
constexpr int kDepthPixels = kDepthSide * kDepthSide;
constexpr float kDepthCurrent = 0.25f;
constexpr float kDepthHistory = 0.5f;
constexpr float kDepthNear = 0.1f;
constexpr float kDepthFar = 1000.0f;
constexpr float kDepthForeground = 2.0f;
constexpr float kDepthBackground = 10.0f;
constexpr float kDepthPixelTolerance = 2.0f;  // byte rounding after the resolve
constexpr int kDepthColorByte = 64;
constexpr float kDepthEnabled = 1.0f;
constexpr float kDepthDisabled = 0.0f;
constexpr float kDepthHardware = 0.5f;
constexpr float kDepthInvalid = -1024.0f;          // the surface was behind the previous camera
constexpr int kStripeColumn = kDepthSide / 2 - 2;  // beyond the 3x3 previous depths the center pixel checks
constexpr float kStripeShift = 2.0f / kDepthSide;  // a jitter difference that brings the stripe to the center
constexpr float kHalfTexel = 0.5f;

using DepthPixels = std::array<Vector4, kDepthPixels>;

float encoded_depth(float meters) { return std::log2(1.0f + meters); }

Texture2D depth_texture(const DepthPixels& pixels) {
  Image image{const_cast<Vector4*>(pixels.data()), kDepthSide, kDepthSide, 1, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32};
  return LoadTextureFromImage(image);
}

DepthPixels filled_depth(Vector4 value) {
  DepthPixels pixels{};
  pixels.fill(value);
  return pixels;
}

DepthPixels current_depth_pixels() {
  DepthPixels pixels = filled_depth({kDepthCurrent, kDepthCurrent, kDepthCurrent, 1.0f});
  pixels[static_cast<std::size_t>((kDepthSide / 2) * kDepthSide + kDepthSide / 2 - 1)] = {1, 1, 1, 1};
  pixels[static_cast<std::size_t>((kDepthSide / 2) * kDepthSide + kDepthSide / 2 + 1)] = {0, 0, 0, 1};
  return pixels;
}

void depth_uniform(const Shader& shader, const char* name, float value) {
  SetShaderValue(shader, GetShaderLocation(shader, name), &value, SHADER_UNIFORM_FLOAT);
}

void depth_vector(const Shader& shader, const char* name, Vector2 value) {
  SetShaderValue(shader, GetShaderLocation(shader, name), &value, SHADER_UNIFORM_VEC2);
}

struct DepthInput {
  float expected = encoded_depth(kDepthBackground);
  float held = encoded_depth(kDepthBackground);
  float hardware = kDepthHardware;
  float valid = kDepthEnabled;
  Vector2 current_jitter{};
  Vector2 previous_jitter{};
  std::optional<float> stripe;  // the previous depth of one column of history, kStripeColumn
  float stripe_color = kDepthHistory;
  Vector2 motion{};  // in texture coordinates
  std::optional<float> center;
  float near = kDepthNear;
  float slope = 0.0f;  // the previous depth's change for each texel across, the same surface's now and before
  int stripe_column = kStripeColumn;
  // The pixel right of the center: a nearer surface (`neighbour_depth`) moving this much, whose
  // previous depth the history's `foreground_columns` leftmost columns hold.
  std::optional<Vector2> neighbour_motion;
  float neighbour_depth = encoded_depth(kDepthForeground);
  int foreground_columns = 0;
};

constexpr float kDepthNearer = 0.3f;  // hardware depth of the neighbour, nearer than kDepthHardware
constexpr std::size_t kNeighbourPixel = (kDepthSide / 2) * kDepthSide + kDepthSide / 2 + 1;

void depth_inputs(const Shader& shader, const std::array<Texture2D, 4>& textures, const DepthInput& input) {
  SetShaderValueTexture(shader, GetShaderLocation(shader, "historyTexture"), textures[1]);
  SetShaderValueTexture(shader, GetShaderLocation(shader, "motionTexture"), textures[2]);
  SetShaderValueTexture(shader, GetShaderLocation(shader, "motionDepth"), textures[3]);
  SetShaderValueMatrix(shader, GetShaderLocation(shader, "reprojection"), MatrixIdentity());
  depth_vector(shader, "texelSize", {1.0f / kDepthSide, 1.0f / kDepthSide});
  depth_vector(shader, "depthProjection",
               {-(kDepthFar + input.near) / (kDepthFar - input.near),
                -2.0f * kDepthFar * input.near / (kDepthFar - input.near)});
  depth_vector(shader, "currentJitter", input.current_jitter);
  depth_vector(shader, "previousJitter", input.previous_jitter);
  depth_uniform(shader, "historyValid", input.valid);
}

// Where, in texels across, the center pixel's surface was in the frame before.
float previous_center(const DepthInput& input) {
  return static_cast<float>(kDepthSide / 2) + kHalfTexel - input.motion.x * kDepthSide;
}

Vector4 history_texel(const DepthInput& input, int x) {
  float held = input.held + input.slope * (static_cast<float>(x) + kHalfTexel - previous_center(input));
  if (x < input.foreground_columns) held = input.neighbour_depth;
  if (x != input.stripe_column) return {kDepthHistory, kDepthHistory, kDepthHistory, held};
  const float c = input.stripe_color;
  return {c, c, c, input.stripe.value_or(held)};
}

DepthPixels history_depth_pixels(const DepthInput& input) {
  DepthPixels pixels{};
  for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = history_texel(input, static_cast<int>(i % kDepthSide));
  return pixels;
}

DepthPixels motion_depth_pixels(const DepthInput& input) {
  DepthPixels pixels{};
  for (std::size_t i = 0; i < pixels.size(); ++i) {
    const auto across = static_cast<float>(static_cast<int>(i % kDepthSide) - kDepthSide / 2);
    pixels[i] = {input.motion.x, input.motion.y, input.expected + input.slope * across, 0};
  }
  if (input.center) {
    pixels[static_cast<std::size_t>((kDepthSide / 2) * kDepthSide + kDepthSide / 2)].z = *input.center;
  }
  if (input.neighbour_motion) {
    pixels[kNeighbourPixel] = {input.neighbour_motion->x, input.neighbour_motion->y, input.neighbour_depth, 0};
  }
  return pixels;
}

DepthPixels hardware_pixels(const DepthInput& input) {
  DepthPixels pixels = filled_depth({input.hardware, 0, 0, 0});
  if (input.neighbour_motion) pixels[kNeighbourPixel].x = kDepthNearer;
  return pixels;
}

std::array<Texture2D, 4> depth_textures(const DepthInput& input) {
  const std::array<Texture2D, 4> textures{
      depth_texture(current_depth_pixels()), depth_texture(history_depth_pixels(input)),
      depth_texture(motion_depth_pixels(input)), depth_texture(hardware_pixels(input))};
  SetTextureFilter(textures[1], TEXTURE_FILTER_BILINEAR);  // as the renderer's history targets are
  return textures;
}

::Color depth_output_pixel(const RenderTexture2D& output) {
  Image image = LoadImageFromTexture(output.texture);
  ImageFlipVertical(&image);
  const ::Color pixel = GetImageColor(image, kDepthSide / 2, kDepthSide / 2);
  UnloadImage(image);
  return pixel;
}

struct DepthFixture {
  redgem::test::RenderStage context;
  Shader shader{};
  std::array<Texture2D, 4> textures{};
  RenderTexture2D output{};

  explicit DepthFixture(const DepthInput& input) {
    const std::string source = redgem::shaders::for_this_gpu(redgem::shaders::kTemporalFragment);
    shader = LoadShaderFromMemory(nullptr, source.c_str());
    textures = depth_textures(input);
    output = LoadRenderTexture(kDepthSide, kDepthSide);
  }

  ~DepthFixture() {
    UnloadRenderTexture(output);
    for (const Texture2D texture : textures) UnloadTexture(texture);
    UnloadShader(shader);
  }

  ::Color draw(const DepthInput& input) const {
    BeginTextureMode(output);
    rlDisableColorBlend();
    BeginShaderMode(shader);
    depth_inputs(shader, textures, input);
    DrawTexture(textures[0], 0, 0, WHITE);
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();
    return depth_output_pixel(output);
  }
};

::Color resolved_depth_pixel(const DepthInput& input) {
  const DepthFixture fixture(input);
  return fixture.draw(input);
}

::Color resolved_depth_pixel(float expected, float held, float hardware, float valid) {
  return resolved_depth_pixel(
      DepthInput{expected, held, hardware, valid, {}, {}, std::nullopt, kDepthHistory, {}, std::nullopt});
}

void check_current_depth_pixel(::Color pixel) {
  CHECK(std::abs(static_cast<float>(pixel.r) - kDepthColorByte) <= kDepthPixelTolerance);
}

}  // namespace

TEST_CASE("temporal depth validation rejects a newly revealed surface even with zero motion split") {
  check_current_depth_pixel(resolved_depth_pixel(encoded_depth(kDepthBackground), encoded_depth(kDepthForeground),
                                                 kDepthHardware, kDepthEnabled));
}

TEST_CASE("temporal depth validation keeps history from a moving surface's previous position") {
  const ::Color pixel = resolved_depth_pixel(encoded_depth(kDepthBackground), encoded_depth(kDepthBackground),
                                             kDepthHardware, kDepthEnabled);
  CHECK(pixel.r > kDepthColorByte + kDepthPixelTolerance);
}

TEST_CASE("temporal depth validation rejects an uninitialized frame and a point behind the previous camera") {
  check_current_depth_pixel(resolved_depth_pixel(encoded_depth(kDepthBackground), encoded_depth(kDepthBackground),
                                                 kDepthHardware, kDepthDisabled));
  check_current_depth_pixel(
      resolved_depth_pixel(kDepthInvalid, encoded_depth(kDepthBackground), kDepthHardware, kDepthEnabled));
}

TEST_CASE("temporal depth validation distinguishes sky from a previously drawn surface") {
  check_current_depth_pixel(resolved_depth_pixel(0, encoded_depth(kDepthForeground), 1.0f, kDepthEnabled));
  const ::Color pixel = resolved_depth_pixel(0, 0, 1.0f, kDepthEnabled);
  CHECK(pixel.r > kDepthColorByte + kDepthPixelTolerance);
}

TEST_CASE("temporal depth validation samples the previous jitter phase without interpolating across depth edges") {
  DepthInput input;
  input.expected = encoded_depth(kDepthForeground);
  input.stripe = input.expected;
  input.previous_jitter = {-kStripeShift, 0};
  CHECK(resolved_depth_pixel(input).r > kDepthColorByte + kDepthPixelTolerance);
  input.previous_jitter = {};
  input.current_jitter = {kStripeShift, 0};
  CHECK(resolved_depth_pixel(input).r > kDepthColorByte + kDepthPixelTolerance);
  input.current_jitter = {};
  check_current_depth_pixel(resolved_depth_pixel(input));
}

TEST_CASE("temporal depth history initializes depth even when color accumulation is rejected") {
  DepthInput input;
  input.valid = kDepthDisabled;
  const ::Color pixel = resolved_depth_pixel(input);
  const float distance = 2.0f * kDepthNear * kDepthFar / (kDepthFar + kDepthNear);
  const float alpha = encoded_depth(distance) * 255.0f;
  CHECK(std::abs(static_cast<float>(pixel.a) - alpha) < kDepthPixelTolerance);
}

TEST_CASE("temporal depth validation retains finite distant history and rejects a distant depth discontinuity") {
  constexpr float kDistantMeters = 1.0e18f;  // farther than half-float linear depth can represent
  const float depth = encoded_depth(kDistantMeters);
  CHECK(resolved_depth_pixel(depth, depth, kDepthHardware, kDepthEnabled).r > kDepthColorByte + kDepthPixelTolerance);
  check_current_depth_pixel(
      resolved_depth_pixel(depth, encoded_depth(kDistantMeters * 0.5f), kDepthHardware, kDepthEnabled));
}

TEST_CASE("temporal depth validation keeps opaque color paired with its central depth beside a surviving surface") {
  DepthInput input;
  input.expected = encoded_depth(kDepthForeground);
  input.held = input.expected;
  input.center = encoded_depth(kDepthBackground);
  check_current_depth_pixel(resolved_depth_pixel(input));
  input.expected = -input.expected;
  input.held = -input.held;
  check_current_depth_pixel(resolved_depth_pixel(input));
}

TEST_CASE("temporal depth validation preserves deliberately stochastic coverage against sky") {
  DepthInput input;
  input.expected = -encoded_depth(kDepthForeground);
  input.held = 0;
  CHECK(resolved_depth_pixel(input).r > kDepthColorByte + kDepthPixelTolerance);
  input.center = 0;
  input.held = input.expected;
  CHECK(resolved_depth_pixel(input).r > kDepthColorByte + kDepthPixelTolerance);
}

TEST_CASE("temporal depth validation rejects an invalid center even beside stochastic history") {
  DepthInput input;
  input.expected = -encoded_depth(kDepthForeground);
  input.held = input.expected;
  input.center = kDepthInvalid;
  check_current_depth_pixel(resolved_depth_pixel(input));
}

namespace {

// The center pixel's history read this far left, between texels: the spline's leftmost column
// is the stripe, which also lies among the previous depths checked around the read.
constexpr float kBetweenTexels = 0.75f / kDepthSide;
constexpr float kDarkStripe = 0.0f;

// The center pixel's history beside a column of another surface's previous depth, colored `stripe_color`.
::Color beside_depth_edge(float stripe_color) {
  DepthInput input;
  input.stripe = encoded_depth(kDepthForeground);
  input.stripe_color = stripe_color;
  input.motion = {kBetweenTexels, 0};
  return resolved_depth_pixel(input);
}

}  // namespace

TEST_CASE("temporal history beside a depth edge takes no color from the other surface") {
  const ::Color dark = beside_depth_edge(kDarkStripe);
  const ::Color same = beside_depth_edge(kDepthHistory);
  CHECK(same.r > kDepthColorByte + kDepthPixelTolerance);
  CHECK(std::abs(static_cast<int>(dark.r) - static_cast<int>(same.r)) <= static_cast<int>(kDepthPixelTolerance));
}

namespace {

constexpr float kTinyNear = 0.001f;     // meters: the nearest clip distance a camera may use
constexpr float kNearSurface = 0.005f;  // meters: a surface between that near plane and the old sky limit
constexpr float kSurfaceSlope = 0.02f;  // logarithmic depth per texel: a steep, but single, surface
constexpr int kSplineOvershoot = 4;     // bytes the spline's outer tap moves the result, at least

}  // namespace

TEST_CASE("temporal depth validation tells a surface just past a tiny near plane from the sky") {
  DepthInput input;
  input.near = kTinyNear;
  input.expected = encoded_depth(kNearSurface);
  input.held = 0;
  check_current_depth_pixel(resolved_depth_pixel(input));
  input.held = input.expected;
  CHECK(resolved_depth_pixel(input).r > kDepthColorByte + kDepthPixelTolerance);
}

TEST_CASE("temporal history on a sloped surface keeps the sharp spline: every neighbour's depth matches") {
  DepthInput input;
  input.slope = kSurfaceSlope;
  input.motion = {kBetweenTexels, 0};
  input.stripe_color = kDarkStripe;
  const ::Color dark = resolved_depth_pixel(input);
  input.stripe_color = kDepthHistory;
  const ::Color same = resolved_depth_pixel(input);
  CHECK(same.r > kDepthColorByte + kDepthPixelTolerance);
  CHECK(std::abs(static_cast<int>(dark.r) - static_cast<int>(same.r)) >= kSplineOvershoot);
}

namespace {

constexpr float kWideMotion =
    3.0f / kDepthSide;  // the neighbour's motion: its 3x3 of history lies left of the center's
constexpr float kSlowMotion = 0.5f / kDepthSide;  // a slow object moving away from the center pixel
constexpr int kForegroundColumns = 3;

}  // namespace

TEST_CASE("temporal depth validation looks for the pixel's own previous depth where its own motion takes it") {
  DepthInput input;  // the center shows the background; its right neighbour, a nearer surface that moved
  input.neighbour_motion = Vector2{kWideMotion, 0};
  input.foreground_columns = kForegroundColumns;
  CHECK(resolved_depth_pixel(input).r > kDepthColorByte + kDepthPixelTolerance);
}

TEST_CASE("temporal history revealed behind an object that moved away keeps none of the object's color") {
  DepthInput input;  // the center texel held the object, which now lies one pixel right and moves on
  input.neighbour_motion = Vector2{kSlowMotion, 0};
  input.stripe_column = kDepthSide / 2;
  input.stripe = input.neighbour_depth;
  input.stripe_color = kDarkStripe;
  const ::Color object = resolved_depth_pixel(input);
  input.stripe_color = kDepthHistory;
  const ::Color background = resolved_depth_pixel(input);
  CHECK(background.r > kDepthColorByte + kDepthPixelTolerance);
  CHECK(std::abs(static_cast<int>(object.r) - static_cast<int>(background.r)) <=
        static_cast<int>(kDepthPixelTolerance));
}
