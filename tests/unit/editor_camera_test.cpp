#include <doctest/doctest.h>

#include "editor/editor_camera.hpp"
#include "editor/picking.hpp"
#include "scene/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr float kTolerance = 1e-3f;
constexpr float kOneSecond = 1.0f;
constexpr float kFullInput = 1.0f;
constexpr float kQuarterTurnPixels = 360.0f;  // 0.25 degrees per pixel
constexpr Vec3 kTarget{1.0f, 2.0f, 3.0f};
constexpr float kFrameRadius = 2.0f;
constexpr float kRayDistance = 10.0f;
constexpr Vec3 kDoubleScale{2, 2, 2};
constexpr float kBoxHalfSize = 1.0f;  // unit_box() spans -1..1
constexpr float kYawDegrees = 45.0f;

Bounds unit_box() {
  Bounds bounds;
  bounds.include({-1, -1, -1});
  bounds.include({1, 1, 1});
  return bounds;
}

}  // namespace

TEST_CASE("fly camera moves along its own axes") {
  EditorCamera camera;
  camera.look_at(Vec3::zero(), Vec3::forward());
  FlyInput input;
  input.forward = kFullInput;
  input.dt = kOneSecond;
  camera.update(input);
  CHECK(camera.position().z < 0.0f);
  CHECK(std::fabs(camera.position().x) < kTolerance);

  FlyInput look;
  look.look_x = kQuarterTurnPixels;  // drag right: turn right
  camera.update(look);
  CHECK(approx_equal(camera.forward(), Vec3::right(), kTolerance));
}

TEST_CASE("either Shift makes the fly camera fast, and its keys steer it") {
  const auto steered = [](const FlyKeys& keys) {
    FlyInput input;
    steer(input, keys);
    return input;
  };
  FlyKeys keys;
  CHECK_FALSE(steered(keys).fast);
  keys.left_shift = true;
  CHECK(steered(keys).fast);
  keys.left_shift = false;
  keys.right_shift = true;
  CHECK(steered(keys).fast);
  keys.forward = keys.right = keys.down = true;
  const FlyInput input = steered(keys);
  CHECK(input.forward == kFullInput);
  CHECK(input.right == kFullInput);
  CHECK(input.up == -kFullInput);
  // Fast covers more ground in the same time.
  EditorCamera slow;
  EditorCamera fast;
  FlyInput walk;
  walk.forward = kFullInput;
  walk.dt = kOneSecond;
  slow.update(walk);
  walk.fast = true;
  fast.update(walk);
  CHECK(length(fast.position() - EditorCamera().position()) > length(slow.position() - EditorCamera().position()));
}

TEST_CASE("framing keeps the target in the center of the view") {
  EditorCamera camera;
  camera.frame(kTarget, kFrameRadius);
  const Vec3 to_target = normalize(kTarget - camera.position());
  CHECK(approx_equal(to_target, camera.forward(), kTolerance));
  CHECK(length(kTarget - camera.position()) > kFrameRadius);
}

TEST_CASE("rays hit rotated and scaled boxes at the right distance") {
  Transform world;
  world.position = {0, 0, -kRayDistance};
  world.scale = kDoubleScale;
  world.rotation = quat_from_euler_degrees({0, kYawDegrees, 0});
  const PickRay ray{Vec3::zero(), Vec3::forward()};
  const auto hit = intersect_ray_box(ray, world, unit_box());
  REQUIRE(hit.has_value());
  // Turned 45 degrees, the nearest edge is half the face diagonal in front of the center.
  const float half_diagonal = kBoxHalfSize * kDoubleScale.x * std::sqrt(2.0f);
  CHECK(*hit == doctest::Approx(kRayDistance - half_diagonal).epsilon(kTolerance));
  CHECK_FALSE(intersect_ray_box({Vec3::zero(), Vec3::up()}, world, unit_box()).has_value());
  CHECK_FALSE(intersect_ray_box({Vec3::zero(), Vec3::back()}, world, unit_box()).has_value());
}

TEST_CASE("picking returns the closest object under the ray") {
  const LoadResult loaded = load_gltf_scene(fixture("hierarchy.glb"));
  REQUIRE(loaded.ok);
  const Scene scene = Scene::from_desc(loaded.scene);
  const ObjectId child = scene.find("Child");
  const Vec3 center = scene.world_transform(child).position;
  const PickRay ray{center + Vec3{0, 0, kRayDistance}, Vec3::forward()};
  CHECK(pick_object(scene, loaded.scene, ray) == child);
  const PickRay miss{center + Vec3{kRayDistance, 0, kRayDistance}, Vec3::forward()};
  CHECK(pick_object(scene, loaded.scene, miss) == kNoObject);

  Vec3 extent_center;
  float radius = 0.0f;
  object_extent(scene, loaded.scene, child, extent_center, radius);
  CHECK(approx_equal(extent_center, center, kTolerance));
  CHECK(radius > kBoxHalfSize);  // the fixture's 2x scaled cube
}
