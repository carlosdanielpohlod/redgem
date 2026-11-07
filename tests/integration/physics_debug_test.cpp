#include <doctest/doctest.h>

#include <limits>
#include <set>

#include "physics/physics_world.hpp"
#include "scene/scene_config.hpp"
#include "physics/jolt/jolt_debug_geometry.hpp"
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
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
bool is_character_triangle(const PhysicsDebugTriangle& triangle) {
  const bool hero = triangle.object == kHero && triangle.type == BodyType::Character;
  return hero && !triangle.trigger;
}
bool is_trigger_triangle(const PhysicsDebugTriangle& triangle) {
  return triangle.object == kTrigger && triangle.trigger;
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
  desc.world.position = desc.world.position + Vec3{4, 1, -2};
  world->teleport(kBox, desc.world);
  const auto after = bounds_of(world->debug_geometry(), kBox);
  CHECK(approx_equal(after.center() - before.center(), Vec3{4, 1, -2}, kTolerance));
}
TEST_CASE("physics debug: characters and trigger shapes are included and their geometry owns its vertices") {
  auto world = create_jolt_physics_world(Vec3::zero());
  auto hero = body(kHero, "character", "capsule");
  hero.world.position = {0, 3, 0};
  hero.bounds = Bounds{{-0.5f, 0, -0.5f}, {0.5f, 2, 0.5f}, true};
  REQUIRE_FALSE(world->add_object(hero));
  auto trigger = body(kTrigger, "static", "sphere");
  trigger.config.trigger = true;
  trigger.world.position = {4, 3, 0};
  trigger.bounds = Bounds{{-0.5f, 0, -0.5f}, {0.5f, 2, 0.5f}, true};
  REQUIRE_FALSE(world->add_object(trigger));
  const auto geometry = world->debug_geometry();
  bool character_seen = false;
  bool trigger_seen = false;
  for (const auto& triangle : geometry.triangles) {
    character_seen |= is_character_triangle(triangle);
    trigger_seen |= is_trigger_triangle(triangle);
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

TEST_CASE("physics debug: vehicle chassis decorators expose their actual collision triangles") {
  auto world = create_jolt_physics_world(Vec3::zero());
  auto car = body(kBox, "vehicle", "box");
  car.world.position = {0, 3, 0};
  ObjectId wheel = 10;
  constexpr float kAxle = 0.8f;
  constexpr float kRadius = 0.3f;
  constexpr float kWidth = 0.2f;
  for (const float z : {-kAxle, kAxle}) {
    for (const float x : {-kAxle, kAxle}) {
      car.wheels.push_back({{wheel, static_cast<ObjectSerial>(wheel)}, {x, 0, z}, kRadius, kWidth});
      ++wheel;
    }
  }
  REQUIRE_FALSE(world->add_object(car));
  const auto geometry = world->debug_geometry();
  REQUIRE_FALSE(geometry.triangles.empty());
  CHECK(approx_equal(bounds_of(geometry, kBox).center(), car.world.position, kTolerance));
}
TEST_CASE("physics debug: transformed compound shapes expose every leaf") {
  auto world = create_jolt_physics_world(Vec3::zero());
  JPH::StaticCompoundShapeSettings settings;
  constexpr float kOffset = 2;
  constexpr float kHalfSize = 0.5f;
  for (const float x : {-kOffset, kOffset})
    settings.AddShape(JPH::Vec3(x, 0, 0), JPH::Quat::sIdentity(), new JPH::BoxShape(JPH::Vec3::sReplicate(kHalfSize)));
  const auto shape = settings.Create();
  REQUIRE_FALSE(shape.HasError());
  const JPH::TransformedShape transformed(JPH::RVec3::sZero(), JPH::Quat::sIdentity(), shape.Get().GetPtr(),
                                          JPH::BodyID());
  PhysicsDebugGeometry geometry;
  const PhysicsDebugTriangle metadata{kBox, BodyType::Static, false, {}};
  REQUIRE(jolt::append_debug_shape(transformed, metadata, 0, kMaxPhysicsDebugTriangles, geometry));
  const auto bounds = bounds_of(geometry, kBox);
  CHECK(bounds.min.x == doctest::Approx(-kOffset - kHalfSize));
  CHECK(bounds.max.x == doctest::Approx(kOffset + kHalfSize));
}

TEST_CASE("physics debug: small colliders hand their unused quota to a detailed mesh") {
  auto world = create_jolt_physics_world(Vec3::zero());
  constexpr ObjectId kBoxes = 10;
  constexpr ObjectId kTerrain = 20;
  for (ObjectId id = 0; id < kBoxes; ++id) REQUIRE_FALSE(world->add_object(body(id, "static", "box")));
  constexpr std::size_t kTriangles = 600;
  constexpr float kSpacing = 2;
  MeshDesc mesh;
  PrimitiveDesc primitive;
  for (std::size_t index = 0; index < kTriangles; ++index) {
    const float x = static_cast<float>(index) * kSpacing;
    const auto base = static_cast<std::uint32_t>(primitive.positions.size() / config::kFloatsPerPosition);
    primitive.positions.insert(primitive.positions.end(), {x, 0, 0, x + 1, 0, 0, x, 0, 1});
    primitive.normals.insert(primitive.normals.end(), {0, 1, 0, 0, 1, 0, 0, 1, 0});
    primitive.indices.insert(primitive.indices.end(), {base, base + 2, base + 1});
  }
  mesh.primitives.push_back(primitive);
  auto terrain = body(kTerrain, "static", "mesh");
  terrain.mesh = &mesh;
  REQUIRE_FALSE(world->add_object(terrain));
  const auto geometry = world->debug_geometry();
  std::size_t seen = 0;
  for (const auto& triangle : geometry.triangles)
    if (triangle.object == kTerrain) ++seen;
  CHECK(seen == kTriangles);
  CHECK_FALSE(geometry.truncated);
}
