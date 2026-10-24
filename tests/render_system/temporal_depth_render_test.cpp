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
  Vector2 motion{};
  std::optional<float> center;
};

void depth_inputs(const Shader& shader, const std::array<Texture2D, 4>& textures, const DepthInput& input) {
  SetShaderValueTexture(shader, GetShaderLocation(shader, "historyTexture"), textures[1]);
  SetShaderValueTexture(shader, GetShaderLocation(shader, "motionTexture"), textures[2]);
  SetShaderValueTexture(shader, GetShaderLocation(shader, "motionDepth"), textures[3]);
  SetShaderValueMatrix(shader, GetShaderLocation(shader, "reprojection"), MatrixIdentity());
  depth_vector(shader, "texelSize", {1.0f / kDepthSide, 1.0f / kDepthSide});
  depth_vector(shader, "depthProjection",
               {-(kDepthFar + kDepthNear) / (kDepthFar - kDepthNear),
                -2.0f * kDepthFar * kDepthNear / (kDepthFar - kDepthNear)});
  depth_vector(shader, "currentJitter", input.current_jitter);
  depth_vector(shader, "previousJitter", input.previous_jitter);
  depth_uniform(shader, "historyValid", input.valid);
}

DepthPixels history_depth_pixels(const DepthInput& input) {
  DepthPixels pixels = filled_depth({kDepthHistory, kDepthHistory, kDepthHistory, input.held});
  if (!input.stripe) return pixels;
  const float c = input.stripe_color;
  for (int y = 0; y < kDepthSide; ++y) {
    pixels[static_cast<std::size_t>(y * kDepthSide + kStripeColumn)] = {c, c, c, *input.stripe};
  }
  return pixels;
}

DepthPixels motion_depth_pixels(const DepthInput& input) {
  DepthPixels pixels = filled_depth({input.motion.x, input.motion.y, input.expected, 0});
  if (input.center) {
    pixels[static_cast<std::size_t>((kDepthSide / 2) * kDepthSide + kDepthSide / 2)].z = *input.center;
  }
  return pixels;
}

std::array<Texture2D, 4> depth_textures(const DepthInput& input) {
  const std::array<Texture2D, 4> textures{
      depth_texture(current_depth_pixels()), depth_texture(history_depth_pixels(input)),
      depth_texture(motion_depth_pixels(input)), depth_texture(filled_depth({input.hardware, 0, 0, 0}))};
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
