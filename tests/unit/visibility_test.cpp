#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "render/render_math.hpp"
#include "render/visibility.hpp"
#include "test_support.hpp"

#include <raymath.h>  // after raylib.h (through visibility.hpp): it relies on raylib's own types

using namespace redgem;
using namespace redgem::render_math;
using redgem::test::kTolerance;

namespace {

constexpr Vec3 kPlace{10.0f, 2.0f, -5.0f};  // somewhere in the world, meters
constexpr Vec3 kTwiceAsBig{2.0f, 2.0f, 2.0f};
constexpr float kDouble = 2.0f;
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

}  // namespace

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
