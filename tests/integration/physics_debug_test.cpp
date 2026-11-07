#include <doctest/doctest.h>

#include <limits>
#include <set>

#include "physics/physics_world.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace {
constexpr float kTolerance = 0.001f;
constexpr ObjectId kBox = 1;
constexpr ObjectId kSphere = 2;
constexpr ObjectId kHero = 3;
constexpr ObjectId kTrigger = 4;
BodyDesc body(ObjectId object, const std::string& type, const std::string& collider) {
  BodyDesc desc;
  desc.object = object;
  desc.config = body_config_from_props({{"body", type}, {"collider", collider}});
  desc.bounds = Bounds{{-0.5f, -1, -0.5f}, {0.5f, 1, 0.5f}, true};
  return desc;
}
Bounds bounds_of(const PhysicsDebugGeometry& geometry, ObjectId object) {
  Bounds bounds;
  for (const auto& triangle : geometry.triangles) {
    if (triangle.object == object)
      for (const auto& corner : triangle.corners) bounds.include(corner);
  }
  return bounds;
}
}  // namespace
TEST_CASE("physics debug: rotated scaled offset colliders follow actual bodies without changing simulation") {
  auto world = create_jolt_physics_world(Vec3::zero());
  auto desc = body(kBox, "dynamic", "box");
  desc.bounds = Bounds{{2, 0, -1}, {4, 2, 1}, true};
  desc.world.position = {10, 5, -3};
  desc.world.scale = {-2, 3, 0.5f};
  desc.world.rotation = quat_from_axis_angle(Vec3::forward(), kPi * kHalf);
  REQUIRE_FALSE(world->add_object(desc));
  const Vec3 velocity{2, 3, 4};
  world->set_velocity(kBox, velocity);
  const auto geometry = world->debug_geometry();
  REQUIRE_FALSE(geometry.triangles.empty());
  CHECK_FALSE(geometry.truncated);
  const Vec3 expected =
      desc.world.position + rotate(desc.world.rotation, hadamard(desc.bounds.center(), desc.world.scale));
  CHECK(approx_equal(bounds_of(geometry, kBox).center(), expected, kTolerance));
  CHECK(approx_equal(world->velocity(kBox), velocity, kTolerance));
  for (const auto& triangle : geometry.triangles) {
    CHECK(triangle.object == kBox);
    CHECK(triangle.type == BodyType::Dynamic);
    CHECK_FALSE(triangle.trigger);
  }
  const auto before = bounds_of(geometry, kBox);
  desc.world.position += Vec3{4, 1, -2};
  world->teleport(kBox, desc.world);
  const auto after = bounds_of(world->debug_geometry(), kBox);
  CHECK(approx_equal(after.center() - before.center(), Vec3{4, 1, -2}, kTolerance));
}
TEST_CASE("physics debug: characters and trigger shapes are included and their geometry owns its vertices") {
  auto world = create_jolt_physics_world(Vec3::zero());
  auto hero = body(kHero, "character", "capsule");
  hero.world.position = {0, 3, 0};
  REQUIRE_FALSE(world->add_object(hero));
  auto trigger = body(kTrigger, "static", "sphere");
  trigger.config.trigger = true;
  trigger.world.position = {4, 3, 0};
  REQUIRE_FALSE(world->add_object(trigger));
  const auto geometry = world->debug_geometry();
  bool character_seen = false;
  bool trigger_seen = false;
  for (const auto& triangle : geometry.triangles) {
    character_seen |= triangle.object == kHero && triangle.type == BodyType::Character && !triangle.trigger;
    trigger_seen |= triangle.object == kTrigger && triangle.trigger;
  }
  CHECK(character_seen);
  CHECK(trigger_seen);
  CHECK(approx_equal(bounds_of(geometry, kTrigger).center(), trigger.world.position, kTolerance));
  world->remove_object(kHero);
  world->remove_object(kTrigger);
  CHECK(world->debug_geometry().triangles.empty());
  world.reset();
  CHECK(bounds_of(geometry, kHero).valid);
}
TEST_CASE("physics debug: mesh collider triangles use mesh vertices rather than the bounding box") {
  auto world = create_jolt_physics_world(Vec3::zero());
  MeshDesc mesh;
  PrimitiveDesc triangle;
  triangle.positions = {0, 0, 0, 2, 0, 0, 0, 0, 3};
  triangle.normals = {0, 1, 0, 0, 1, 0, 0, 1, 0};
  triangle.indices = {0, 2, 1};
  mesh.primitives.push_back(triangle);
  auto desc = body(kBox, "static", "mesh");
  desc.mesh = &mesh;
  desc.world.position = {1, 2, 3};
  REQUIRE_FALSE(world->add_object(desc));
  const auto geometry = world->debug_geometry();
  REQUIRE(geometry.triangles.size() == 1);
  const Bounds actual = bounds_of(geometry, kBox);
  CHECK(approx_equal(actual.min, Vec3{1, 2, 3}, kTolerance));
  CHECK(approx_equal(actual.max, Vec3{3, 2, 6}, kTolerance));
}
TEST_CASE("physics debug: quotas include later colliders and bound oversized requests") {
  auto world = create_jolt_physics_world(Vec3::zero());
  REQUIRE_FALSE(world->add_object(body(kSphere, "static", "sphere")));
  REQUIRE_FALSE(world->add_object(body(kHero, "static", "capsule")));
  REQUIRE_FALSE(world->add_object(body(kTrigger, "static", "box")));
  constexpr std::size_t kSmallLimit = 9;
  const auto geometry = world->debug_geometry(kSmallLimit);
  CHECK(geometry.triangles.size() <= kSmallLimit);
  CHECK(geometry.truncated);
  std::set<ObjectId> seen;
  for (const auto& triangle : geometry.triangles) seen.insert(triangle.object);
  CHECK(seen == std::set<ObjectId>{kSphere, kHero, kTrigger});
  CHECK(world->debug_geometry(0).triangles.empty());
  const auto huge = world->debug_geometry(std::numeric_limits<std::size_t>::max());
  CHECK(huge.triangles.size() <= kMaxPhysicsDebugTriangles);
}
