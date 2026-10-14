#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "render/render_math.hpp"
#include "test_support.hpp"

#include <raymath.h>  // after raylib.h (through render_math.hpp): it relies on raylib's own types

using namespace redgem;
using namespace redgem::render_math;
using redgem::test::kTolerance;

namespace {

constexpr unsigned char kFullByte = 255;
constexpr unsigned char kHalfByte = 127;  // half of full, rounded down
constexpr unsigned char kFifthByte = 51;  // a fifth of full
constexpr float kFifth = 0.2f;
constexpr float kOverbright = 2.0f;  // more than a color channel can hold
constexpr int kTranslationX = 12;    // where a column-major matrix keeps its translation
constexpr int kTranslationY = 13;
constexpr int kTranslationZ = 14;
constexpr Vec3 kPlace{10.0f, 2.0f, -5.0f};  // somewhere in the world, meters
constexpr Vec3 kTwiceAsBig{2.0f, 2.0f, 2.0f};
constexpr float kDouble = 2.0f;
constexpr Vec3 kParentPlace{10.0f, 0.0f, 0.0f};
constexpr float kIdentity[kMatrixFloats] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
constexpr const char* kCardLine = "    > Renderer: Some Card 9000";
constexpr const char* kCardName = " Some Card 9000";
constexpr const char* kOtherLine = "    > Version:  4.6";
constexpr int kWindowWidth = 1280;  // pixels
constexpr int kQuarter = 4;
constexpr int kQuarterWidth = 320;
constexpr int kTinyTarget = 3;  // pixels: smaller than the factor it is divided by
constexpr float kWind = 0.4f;
constexpr float kMaskCutoff = 0.6f;
constexpr int kSomeImage = 3;        // index of a texture
constexpr float kLightRange = 7.0f;  // meters
constexpr std::size_t kTwoLights = 2;
constexpr std::size_t kTooManyLights = 40;
constexpr float kParticleSize = 2.0f;       // meters across
constexpr float kQuarterTurn = kPi * 0.5f;  // radians
constexpr Vec3 kLowerLeft{-1.0f, -1.0f, 0.0f};
constexpr Vec3 kLowerRight{1.0f, -1.0f, 0.0f};
constexpr Vec3 kUpperRight{1.0f, 1.0f, 0.0f};
constexpr Vec3 kUpperLeft{-1.0f, 1.0f, 0.0f};
constexpr int kPictureWidth = 200;  // pixels: a picture twice as wide as tall
constexpr int kPictureHeight = 100;
constexpr int kAskedWidth = 50;
constexpr int kAskedHeight = 80;
constexpr float kHeightForAskedWidth = 25.0f;
constexpr float kWidthForAskedHeight = 160.0f;
constexpr int kNotAsked = 0;
constexpr float kBoxWidth = 100.0f;  // pixels
constexpr float kBoxHeight = 40.0f;
constexpr float kCornerRadius = 10.0f;
constexpr float kHalfRound = 0.5f;  // a 10 px radius on a 40 px side
constexpr float kHugeRadius = 500.0f;

MaterialDesc material(AlphaMode mode, bool textured) {
  MaterialDesc desc;
  desc.alpha_mode = mode;
  desc.alpha_cutoff = kMaskCutoff;
  if (textured) desc.base_color_image = kSomeImage;
  return desc;
}

}  // namespace

TEST_CASE("colors become bytes, and channels beyond full are held at full") {
  const ::Color color = to_raylib(redgem::Color{1.0f, kHalf, 0.0f, kOverbright});
  CHECK(color.r == kFullByte);
  CHECK(color.g == kHalfByte);
  CHECK(color.b == 0);
  CHECK(color.a == kFullByte);
}

TEST_CASE("a color's bytes are sent to shaders as numbers from 0 to 1") {
  const auto floats = color_floats(::Color{kFullByte, 0, kFifthByte, kFullByte});
  CHECK(floats[0] == doctest::Approx(1.0f));
  CHECK(floats[1] == doctest::Approx(0.0f));
  CHECK(floats[2] == doctest::Approx(kFifth));
  CHECK(floats[3] == doctest::Approx(1.0f));
}

TEST_CASE("vectors keep their numbers going to raylib and back") {
  const Vector3 converted = to_raylib(kPlace);
  CHECK(converted.x == kPlace.x);
  CHECK(converted.y == kPlace.y);
  CHECK(converted.z == kPlace.z);
  CHECK(approx_equal(to_vec3(converted), kPlace, kTolerance));
}

TEST_CASE("a matrix written column by column keeps its translation where raylib looks for it") {
  std::array<float, kMatrixFloats> columns{};
  for (int i = 0; i < kMatrixFloats; ++i) columns[static_cast<std::size_t>(i)] = kIdentity[i];
  columns[kTranslationX] = kPlace.x;
  columns[kTranslationY] = kPlace.y;
  columns[kTranslationZ] = kPlace.z;
  const Matrix matrix = from_column_major(columns);
  CHECK(matrix.m12 == kPlace.x);
  CHECK(matrix.m13 == kPlace.y);
  CHECK(matrix.m14 == kPlace.z);
  CHECK(matrix.m0 == 1.0f);
  CHECK(matrix.m3 == 0.0f);
}

TEST_CASE("a matrix written column by column keeps every number under raylib's name for it") {
  std::array<float, kMatrixFloats> columns{};
  for (int i = 0; i < kMatrixFloats; ++i) columns[static_cast<std::size_t>(i)] = static_cast<float>(i);
  const Matrix matrix = from_column_major(columns);
  // raylib names a matrix's numbers m0..m15 in the same column by column order.
  const float named[] = {matrix.m0, matrix.m1, matrix.m2,  matrix.m3,  matrix.m4,  matrix.m5,  matrix.m6,  matrix.m7,
                         matrix.m8, matrix.m9, matrix.m10, matrix.m11, matrix.m12, matrix.m13, matrix.m14, matrix.m15};
  for (int i = 0; i < kMatrixFloats; ++i) CHECK(named[i] == static_cast<float>(i));
}

TEST_CASE("an object's matrix scales it, then moves it to its place") {
  redgem::Transform transform;
  transform.position = kPlace;
  transform.scale = kTwiceAsBig;
  const Vector3 moved = Vector3Transform({1.0f, 0.0f, 0.0f}, model_matrix(transform));
  CHECK(approx_equal(to_vec3(moved), kPlace + Vec3{kDouble, 0.0f, 0.0f}, kTolerance));
}

TEST_CASE("the graphics card's name is read from the line that names it and from no other") {
  const auto card = graphics_card_name(kCardLine);
  REQUIRE(card.has_value());
  CHECK(*card == kCardName);
  CHECK_FALSE(graphics_card_name(kOtherLine).has_value());
}

TEST_CASE("a smaller target is a fraction of the picture but never empty") {
  CHECK(downscaled(kWindowWidth, kQuarter) == kQuarterWidth);
  CHECK(downscaled(kTinyTarget, kQuarter) == 1);
}

TEST_CASE("opaque materials are drawn whole, still and one-sided") {
  const AlphaTest test = alpha_test(material(AlphaMode::Opaque, true), kWind);
  CHECK(test.cutoff == kNoCutoff);
  CHECK(test.sway == kNoWind);
  CHECK_FALSE(test.two_sided);
}

TEST_CASE("cut-out materials use their own cutoff and sway only when textured") {
  const AlphaTest leaves = alpha_test(material(AlphaMode::Mask, true), kWind);
  CHECK(leaves.cutoff == doctest::Approx(kMaskCutoff));
  CHECK(leaves.sway == doctest::Approx(kWind));
  CHECK(leaves.two_sided);
  const AlphaTest plain = alpha_test(material(AlphaMode::Mask, false), kWind);
  CHECK(plain.cutoff == doctest::Approx(kMaskCutoff));
  CHECK(plain.sway == kNoWind);
  CHECK(plain.two_sided);
}

TEST_CASE("textured blend materials are cut out like foliage; untextured ones are drawn whole") {
  const AlphaTest fence = alpha_test(material(AlphaMode::Blend, true), kWind);
  CHECK(fence.cutoff == doctest::Approx(kBlendAsCutoutThreshold));
  CHECK(fence.sway == doctest::Approx(kWind));
  CHECK(fence.two_sided);
  const AlphaTest glass = alpha_test(material(AlphaMode::Blend, false), kWind);
  CHECK(glass.cutoff == kNoCutoff);
  CHECK(glass.sway == kNoWind);
  CHECK_FALSE(glass.two_sided);
}

TEST_CASE("double-sided materials are drawn without backface culling") {
  MaterialDesc sheet = material(AlphaMode::Opaque, false);
  sheet.double_sided = true;
  CHECK(alpha_test(sheet, kWind).two_sided);
}

TEST_CASE("point lights are laid out in the order given") {
  const std::vector<PointLight> lights = {{kPlace, Vec3::one(), kLightRange}, {kParentPlace, kTwiceAsBig, kDouble}};
  const PointLightUniforms packed = pack_point_lights(lights);
  REQUIRE(packed.count == static_cast<int>(kTwoLights));
  CHECK(approx_equal(to_vec3(packed.positions[0]), kPlace, kTolerance));
  CHECK(approx_equal(to_vec3(packed.positions[1]), kParentPlace, kTolerance));
  CHECK(approx_equal(to_vec3(packed.colors[1]), kTwiceAsBig, kTolerance));
  CHECK(packed.ranges[0] == doctest::Approx(kLightRange));
  CHECK(packed.ranges[1] == doctest::Approx(kDouble));
}

TEST_CASE("point lights beyond what the shader takes are left out") {
  const std::vector<PointLight> lights(kTooManyLights, PointLight{kPlace, Vec3::one(), kLightRange});
  CHECK(pack_point_lights(lights).count == shaders::kMaxPointLights);
  CHECK(pack_point_lights({}).count == 0);
}

TEST_CASE("a camera's right and up are what a square lies along to face it") {
  const Facing view = facing(Vec3::zero(), Vec3::forward(), Vec3::up());
  CHECK(approx_equal(view.right, Vec3::right(), kTolerance));
  CHECK(approx_equal(view.up, Vec3::up(), kTolerance));
  // A tilted "up" still gives a square that faces the camera.
  const Facing tilted = facing(Vec3::zero(), Vec3::forward(), normalize(Vec3{0.0f, 1.0f, -1.0f}));
  CHECK(approx_equal(tilted.up, Vec3::up(), kTolerance));
}

TEST_CASE("a particle is a square around its place, facing the camera") {
  const Facing view{Vec3::right(), Vec3::up()};
  const auto corners = particle_corners(kPlace, kParticleSize, 0.0f, view);
  CHECK(approx_equal(corners[0], kPlace + kLowerLeft, kTolerance));
  CHECK(approx_equal(corners[1], kPlace + kLowerRight, kTolerance));
  CHECK(approx_equal(corners[2], kPlace + kUpperRight, kTolerance));
  CHECK(approx_equal(corners[3], kPlace + kUpperLeft, kTolerance));
}

TEST_CASE("a turned particle spins around the line of sight") {
  const Facing view{Vec3::right(), Vec3::up()};
  const auto corners = particle_corners(kPlace, kParticleSize, kQuarterTurn, view);
  // A quarter turn: each corner is where the previous one was.
  CHECK(approx_equal(corners[0], kPlace + kLowerRight, kTolerance));
  CHECK(approx_equal(corners[1], kPlace + kUpperRight, kTolerance));
  CHECK(approx_equal(corners[2], kPlace + kUpperLeft, kTolerance));
  CHECK(approx_equal(corners[3], kPlace + kLowerLeft, kTolerance));
}

TEST_CASE("particles are batched as plain dots first, then each picture once") {
  const int smoke = 0;
  const int star = 0;
  const std::vector<const int*> worn = {&smoke, nullptr, &star, &smoke};
  const std::vector<const int*> batches = picture_batches(worn);
  CHECK(batches == std::vector<const int*>{nullptr, &smoke, &star});
  CHECK(picture_batches(std::vector<const int*>{}) == std::vector<const int*>{nullptr});
}

TEST_CASE("an image without a size is drawn as large as its picture") {
  const Size size = image_size(kNotAsked, kNotAsked, kPictureWidth, kPictureHeight);
  CHECK(size.width == doctest::Approx(static_cast<float>(kPictureWidth)));
  CHECK(size.height == doctest::Approx(static_cast<float>(kPictureHeight)));
}

TEST_CASE("an image given one size keeps its picture's shape") {
  const Size by_width = image_size(kAskedWidth, kNotAsked, kPictureWidth, kPictureHeight);
  CHECK(by_width.width == doctest::Approx(static_cast<float>(kAskedWidth)));
  CHECK(by_width.height == doctest::Approx(kHeightForAskedWidth));
  const Size by_height = image_size(kNotAsked, kAskedHeight, kPictureWidth, kPictureHeight);
  CHECK(by_height.width == doctest::Approx(kWidthForAskedHeight));
  CHECK(by_height.height == doctest::Approx(static_cast<float>(kAskedHeight)));
}

TEST_CASE("an image given both sizes is stretched to them") {
  const Size size = image_size(kAskedWidth, kAskedHeight, kPictureWidth, kPictureHeight);
  CHECK(size.width == doctest::Approx(static_cast<float>(kAskedWidth)));
  CHECK(size.height == doctest::Approx(static_cast<float>(kAskedHeight)));
}

TEST_CASE("a rectangle's corners are as round as its radius is to its shorter side") {
  CHECK(corner_roundness(kCornerRadius, kBoxWidth, kBoxHeight) == doctest::Approx(kHalfRound));
  CHECK(corner_roundness(kCornerRadius, kBoxHeight, kBoxWidth) == doctest::Approx(kHalfRound));
  CHECK(corner_roundness(kHugeRadius, kBoxWidth, kBoxHeight) == kFullyRound);
}

TEST_CASE("a rectangle without a radius, or without a size, has square corners") {
  CHECK(corner_roundness(0.0f, kBoxWidth, kBoxHeight) == kSquareCorners);
  CHECK(corner_roundness(kCornerRadius, 0.0f, kBoxHeight) == kSquareCorners);
}
