#include <doctest/doctest.h>

#include <limits>

#include "editor/scene_overlays.hpp"
#include "physics/body_config.hpp"
#include "render/debug_draw.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr Vec3 kCrateMin{-2.0f, 0.0f, -1.0f};
constexpr Vec3 kCrateMax{2.0f, 3.0f, 1.0f};

SceneDesc scene_with_a_crate() {
  SceneDesc desc;
  MeshDesc crate;
  crate.bounds = Bounds{kCrateMin, kCrateMax, true};
  desc.meshes.push_back(crate);
  return desc;
}

}  // namespace

TEST_CASE("an object is boxed by its mesh, and an object without a mesh by a small box") {
  const SceneDesc desc = scene_with_a_crate();
  GameObject crate;
  crate.mesh = 0;
  const Bounds boxed = bounds_of(crate, desc);
  CHECK(approx_equal(boxed.min, kCrateMin, kTolerance));
  CHECK(approx_equal(boxed.max, kCrateMax, kTolerance));
  GameObject empty;
  const Bounds small = bounds_of(empty, desc);
  CHECK(approx_equal(small.min, default_collider_bounds().min, kTolerance));
  CHECK(approx_equal(small.max, default_collider_bounds().max, kTolerance));
}

TEST_CASE("the viewport colors come out as raylib takes them") {
  constexpr unsigned char kOrange[] = {255, 160, 0, 128};
  const ::Color color = rgba(kOrange);
  CHECK(color.r == kOrange[0]);
  CHECK(color.g == kOrange[1]);
  CHECK(color.b == kOrange[2]);
  CHECK(color.a == kOrange[3]);
}

TEST_CASE("collider wire commands keep one shared edge and draw in the world pass") {
  PhysicsDebugGeometry geometry;
  const PhysicsDebugTriangle first{1, BodyType::Dynamic, false, {Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0}}};
  const PhysicsDebugTriangle second{1, BodyType::Dynamic, false, {Vec3{1, 0, 0}, Vec3{1, 1, 0}, Vec3{0, 1, 0}}};
  geometry.triangles = {first, second};
  DebugDrawBuffer commands;
  REQUIRE(append_physics_colliders(commands, geometry));
  constexpr std::size_t kUniqueEdges = 5;
  REQUIRE(commands.items().size() == kUniqueEdges);
  for (const auto& command : commands.items()) CHECK(command.kind == DebugDrawItem::Kind::Line);
}

TEST_CASE("collider wires discard whole nonfinite triangles before sorting") {
  const PhysicsDebugTriangle valid{1, BodyType::Dynamic, false, {Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0}}};
  auto nan = valid;
  nan.corners[0].x = std::numeric_limits<float>::quiet_NaN();
  auto infinity = valid;
  infinity.corners[1].y = std::numeric_limits<float>::infinity();
  PhysicsDebugGeometry geometry;
  geometry.triangles = {nan, valid, infinity};
  DebugDrawBuffer commands;
  CHECK(append_physics_colliders(commands, geometry));
  CHECK(commands.items().size() == kDebugTriangleCorners);
}

TEST_CASE("collider lines fill free capacity while preserving script commands") {
  constexpr std::size_t kQuadEdges = 5;
  DebugDrawBuffer commands;
  const DebugDrawItem script{DebugDrawItem::Kind::Box, Vec3::zero(), Vec3::one(), Color{1, 0, 0, 1}, true};
  const auto script_count = kMaxDebugDrawCommands - kQuadEdges;
  for (std::size_t index = 0; index < script_count; ++index) REQUIRE(commands.append(script));
  PhysicsDebugGeometry geometry;
  geometry.triangles = {{1, BodyType::Dynamic, false, {Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0}}},
                        {1, BodyType::Dynamic, false, {Vec3{1, 0, 0}, Vec3{1, 1, 0}, Vec3{0, 1, 0}}}};
  REQUIRE(physics_debug_triangle_budget(commands) >= geometry.triangles.size());
  REQUIRE(append_physics_colliders(commands, geometry));
  CHECK(commands.items().size() == kMaxDebugDrawCommands);
  for (std::size_t index = 0; index < script_count; ++index)
    CHECK(commands.items()[index].kind == DebugDrawItem::Kind::Box);
  CHECK_FALSE(append_physics_colliders(commands, geometry));
  CHECK(physics_debug_triangle_budget(commands) > 0);
  CHECK(append_physics_colliders(commands, PhysicsDebugGeometry{}));
}
