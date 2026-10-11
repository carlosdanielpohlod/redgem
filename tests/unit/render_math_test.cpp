#include <doctest/doctest.h>

#include <cmath>
#include <limits>
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
constexpr Vec3 kParentPlace{10.0f, 0.0f, 0.0f};
constexpr Vec3 kChildOffset{0.0f, 2.0f, 0.0f};
constexpr Vec3 kChildPlace{10.0f, 2.0f, 0.0f};
constexpr Vec3 kMovedParent{20.0f, 0.0f, 0.0f};
constexpr Vec3 kMovedChild{20.0f, 2.0f, 0.0f};
constexpr ObjectId kParent = 0;
constexpr ObjectId kChild = 1;
constexpr Vec3 kUnitBoxMin{-1.0f, -1.0f, -1.0f};
constexpr Vec3 kUnitBoxMax{1.0f, 1.0f, 1.0f};
constexpr Vec3 kCornerBoxMax{2.0f, 2.0f, 2.0f};  // a box from the origin: its center is at (1, 1, 1)
constexpr Vec3 kCornerBoxCenterDoubled{2.0f, 2.0f, 2.0f};
const float kUnitBoxRadius = std::sqrt(3.0f);  // half the diagonal of a 2 m cube
constexpr float kBoneShift = 5.0f;             // meters the first bone moved the mesh along x
constexpr float kAskedAngle = 0.03f;           // radians
constexpr int kMeshCount = 4;
constexpr int kKnownMesh = 2;
constexpr int kUnknownMesh = 9;
constexpr float kIdentity[kMatrixFloats] = {1, 0, 0, 0, 0, 1, 0, 0,
                                            0, 0, 1, 0, 0, 0, 0, 1};  // views -1..1 on every axis
constexpr float kSmallRadius = 0.25f;
constexpr float kMinAngle = 0.01f;
constexpr Vec3 kFarViewer{0.0f, 0.0f, 500.0f};
constexpr Vec3 kNearViewer{0.0f, 0.0f, 3.0f};
constexpr Vec3 kOutsideView{4.0f, 0.0f, 0.0f};
constexpr float kLodFrom = 10.0f;  // meters
constexpr float kLodTo = 50.0f;
const float kForever = std::numeric_limits<float>::infinity();
constexpr float kShadowsAsked = 0.8f;
constexpr Vec3 kHighSun{0.0f, 1.0f, 0.0f};
constexpr Vec3 kSlantedSun{0.6f, 0.8f, 0.0f};
constexpr Vec3 kSetSun{1.0f, 0.0f, 0.0f};
constexpr float kStep = 0.5f;
constexpr float kAboveStep = 7.3f;
constexpr float kSnapped = 7.0f;
constexpr float kBelowZero = -0.2f;
constexpr float kSnappedBelowZero = -0.5f;
constexpr Vector3 kOrigin{0.0f, 0.0f, 0.0f};
constexpr Vector3 kLookingAhead{0.0f, 0.0f, -1.0f};
constexpr float kTexel = kShadowExtent / static_cast<float>(kShadowMapSize);  // meters of ground per shadow texel
constexpr float kAheadOfCamera = kShadowExtent * kShadowFocusAhead;           // meters
constexpr float kQuarterTexel = kTexel * 0.25f;
constexpr float kHalfTexel = kTexel * 0.5f;
constexpr float kTexelAndAHalf = kTexel * 1.5f;
constexpr float kLowCamera = 0.3f;  // meters above the floor
constexpr float kHighCamera = 8.0f;
constexpr int kLastFace = kCubeFaces - 1;
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

SceneDesc parent_and_child() {
  SceneDesc desc;
  NodeDesc parent;
  parent.local.position = kParentPlace;
  parent.children = {kChild};
  NodeDesc child;
  child.parent = kParent;
  child.local.position = kChildOffset;
  desc.nodes = {parent, child};
  return desc;
}

Bounds box(Vec3 min, Vec3 max) {
  Bounds bounds;
  bounds.min = min;
  bounds.max = max;
  bounds.valid = true;
  return bounds;
}

GameObject drawable_object() {
  GameObject object;
  object.mesh = kKnownMesh;
  return object;
}

View identity_view(Vec3 viewer) { return {frustum_from_matrix(kIdentity), viewer, kMinAngle}; }

Vec3 across_the_map(Vector3 v) { return {v.x, 0.0f, v.z}; }

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

TEST_CASE("every object is placed in the world through its parent") {
  Scene scene = Scene::from_desc(parent_and_child());
  std::vector<redgem::Transform> worlds;
  world_transforms(scene, worlds);
  REQUIRE(worlds.size() == scene.size());
  CHECK(approx_equal(worlds[kParent].position, kParentPlace, kTolerance));
  CHECK(approx_equal(worlds[kChild].position, kChildPlace, kTolerance));

  scene.at(kParent).local.position = kMovedParent;
  world_transforms(scene, worlds);
  CHECK(approx_equal(worlds[kChild].position, kMovedChild, kTolerance));
}

TEST_CASE("a destroyed object keeps the place it had") {
  Scene scene = Scene::from_desc(parent_and_child());
  std::vector<redgem::Transform> worlds;
  world_transforms(scene, worlds);
  scene.at(kChild).destroyed = true;
  scene.at(kChild).local.position = Vec3::zero();
  world_transforms(scene, worlds);
  CHECK(approx_equal(worlds[kChild].position, kChildPlace, kTolerance));
}

TEST_CASE("the sphere around a mesh follows its object's place and scale") {
  redgem::Transform world;
  world.position = kPlace;
  world.scale = kTwiceAsBig;
  const BoundingSphere centered = world_sphere(box(kUnitBoxMin, kUnitBoxMax), world);
  CHECK(approx_equal(centered.center, kPlace, kTolerance));
  CHECK(centered.radius == doctest::Approx(kUnitBoxRadius * kDouble));
  // A mesh that is not centered on its own origin: the scale moves its middle too.
  const BoundingSphere off_center = world_sphere(box(Vec3::zero(), kCornerBoxMax), world);
  CHECK(approx_equal(off_center.center, kPlace + kCornerBoxCenterDoubled, kTolerance));
}

TEST_CASE("the sphere around a skinned mesh follows its first bone, with room for its limbs") {
  const Bounds bounds = box(kUnitBoxMin, kUnitBoxMax);
  const BoundingSphere moved = skinned_sphere(bounds, MatrixTranslate(kBoneShift, 0.0f, 0.0f));
  CHECK(approx_equal(moved.center, Vec3{kBoneShift, 0.0f, 0.0f}, kTolerance));
  CHECK(moved.radius == doctest::Approx(kUnitBoxRadius * kSkinnedBoundsMargin));
  const BoundingSphere scaled = skinned_sphere(bounds, MatrixScale(kDouble, kDouble, kDouble));
  CHECK(scaled.radius == doctest::Approx(kUnitBoxRadius * kSkinnedBoundsMargin * kDouble));
}

TEST_CASE("each pass has its own smallest size worth drawing, unless one is asked for") {
  CHECK(smallest_angle_drawn(0.0f, false) == kMinVisibleAngle);
  CHECK(smallest_angle_drawn(0.0f, true) == kMinShadowAngle);
  CHECK(smallest_angle_drawn(kAskedAngle, false) == kAskedAngle);
  CHECK(smallest_angle_drawn(kAskedAngle, true) == kAskedAngle);
  CHECK(kMinShadowAngle > kMinVisibleAngle);  // shadows of small things disappear sooner
}

TEST_CASE("only live, visible objects with a known mesh are drawn") {
  CHECK(has_something_to_draw(drawable_object(), false, kMeshCount));
  GameObject destroyed = drawable_object();
  destroyed.destroyed = true;
  CHECK_FALSE(has_something_to_draw(destroyed, false, kMeshCount));
  GameObject hidden = drawable_object();
  hidden.visible = false;
  CHECK_FALSE(has_something_to_draw(hidden, false, kMeshCount));
  GameObject empty = drawable_object();
  empty.mesh = kNoIndex;
  CHECK_FALSE(has_something_to_draw(empty, false, kMeshCount));
  GameObject unknown = drawable_object();
  unknown.mesh = kUnknownMesh;
  CHECK_FALSE(has_something_to_draw(unknown, false, kMeshCount));
}

TEST_CASE("prefabs are drawn in the editor only") {
  GameObject prefab = drawable_object();
  prefab.prefab = true;
  CHECK_FALSE(has_something_to_draw(prefab, false, kMeshCount));
  CHECK(has_something_to_draw(prefab, true, kMeshCount));
}

TEST_CASE("a sphere is worth drawing when it is in view, big enough and at its distance") {
  const BoundingSphere middle{Vec3::zero(), kSmallRadius};
  CHECK(worth_drawing(identity_view(kNearViewer), middle, 0.0f, kForever));
  CHECK_FALSE(worth_drawing(identity_view(kNearViewer), {kOutsideView, kSmallRadius}, 0.0f, kForever));
  CHECK_FALSE(worth_drawing(identity_view(kFarViewer), middle, 0.0f, kForever));  // too small from there
  CHECK_FALSE(
      worth_drawing(identity_view(kNearViewer), middle, kLodFrom, kLodTo));  // a level of detail for farther away
}

TEST_CASE("the sun casts shadows as dark as asked, and none once it has set") {
  CHECK(sun_shadow_strength(kHighSun, kShadowsAsked) == kShadowsAsked);
  CHECK(sun_shadow_strength(kSetSun, kShadowsAsked) == kNoShadows);
  CHECK(sun_shadow_strength(-kHighSun, kShadowsAsked) == kNoShadows);
}

TEST_CASE("values snap down to whole steps, below zero too") {
  CHECK(snapped_down(kAboveStep, kStep) == doctest::Approx(kSnapped));
  CHECK(snapped_down(kSnapped, kStep) == doctest::Approx(kSnapped));
  CHECK(snapped_down(kBelowZero, kStep) == doctest::Approx(kSnappedBelowZero));
}

TEST_CASE("a sun straight overhead orients its shadow map by another axis") {
  const Vector3 overhead = shadow_map_vertical(to_raylib(kHighSun));
  CHECK(approx_equal(to_vec3(overhead), to_vec3(kForwardAxis), kTolerance));
  const Vector3 slanted = shadow_map_vertical(to_raylib(kSlantedSun));
  CHECK(approx_equal(to_vec3(slanted), to_vec3(kUpAxis), kTolerance));
}

TEST_CASE("the sun looks at the ground ahead of the camera, from far back along its light") {
  const SunView view = fit_sun_view(kOrigin, kLookingAhead, kSlantedSun);
  const Vec3 ahead{0.0f, 0.0f, -kAheadOfCamera};
  // Within a texel (the snapping) of the point ahead, on each of the map's two axes.
  CHECK(length(to_vec3(view.target) - ahead) <= kTexel * kDouble);
  CHECK(approx_equal(to_vec3(view.position) - to_vec3(view.target), kSlantedSun * kShadowSunDistance, kTolerance));
  CHECK(approx_equal(to_vec3(view.up), to_vec3(kUpAxis), kTolerance));
}

TEST_CASE("the shadow map moves in whole texels, so its edges do not shimmer") {
  const auto target_from = [](float camera_x) {
    const Vector3 position{camera_x, 0.0f, 0.0f};
    return across_the_map(fit_sun_view(position, Vector3Add(position, kLookingAhead), kHighSun).target);
  };
  // Two camera places within the same texel: the map stays where it is.
  CHECK(approx_equal(target_from(kQuarterTexel), target_from(kHalfTexel), kEpsilon));
  // One texel further: the map follows by exactly one texel.
  const Vec3 moved = target_from(kTexelAndAHalf) - target_from(kHalfTexel);
  CHECK(length(moved) == doctest::Approx(kTexel));
}

TEST_CASE("each cube face's picture is square to the direction it looks in") {
  for (const CubeFace& face : kCubeFaceViews) {
    const FaceAxes axes = face_axes(face);
    CHECK(Vector3DotProduct(axes.right, face.forward) == doctest::Approx(0.0f));
    CHECK(Vector3DotProduct(axes.up, face.forward) == doctest::Approx(0.0f));
    CHECK(Vector3DotProduct(axes.right, axes.up) == doctest::Approx(0.0f));
    CHECK(Vector3Length(axes.right) == doctest::Approx(1.0f));
    CHECK(approx_equal(to_vec3(axes.up), to_vec3(face.up), kTolerance));
  }
}

TEST_CASE("reflections are captured from the camera, but not from below eye height") {
  CHECK(reflection_eye({kPlace.x, kLowCamera, kPlace.z}).y == kReflectionEyeHeight);
  CHECK(reflection_eye({kPlace.x, kHighCamera, kPlace.z}).y == kHighCamera);
  CHECK(reflection_eye({kPlace.x, kLowCamera, kPlace.z}).x == kPlace.x);
  CHECK(reflection_eye({kPlace.x, kLowCamera, kPlace.z}).z == kPlace.z);
}

TEST_CASE("the cube's faces are captured in turn, and reflections wait for all six") {
  CHECK(face_after(0) == 1);
  CHECK(face_after(kLastFace) == 0);
  CHECK(reflection_strength(0) == kReflectionsOff);
  CHECK(reflection_strength(kLastFace) == kReflectionsOff);
  CHECK(reflection_strength(kCubeFaces) == kReflectionsOn);
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
