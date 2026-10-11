#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include "core/config.hpp"
#include "physics/physics_config.hpp"
#include "physics/physics_world.hpp"

using namespace redgem;

namespace {

constexpr float kDt = config::kFixedDeltaSeconds;
constexpr int kSettleSteps = 180;
constexpr float kPositionTolerance = 0.05f;
constexpr Vec3 kAboveGround{0, 5, 0};
constexpr Vec3 kDown{0, -1, 0};
constexpr Vec3 kUp{0, 1, 0};
constexpr float kRayLength = 10.0f;
constexpr Vec3 kFarBelow{0, -50, 0};
constexpr Vec3 kDropHeight{0, 3, 0};
constexpr Vec3 kCrateHalfExtents{0.5f, 0.5f, 0.5f};
constexpr Vec3 kZoneHalfExtents{1, 1, 1};
constexpr int kOneStep = 1;

enum Ids : ObjectId { kGround = 0, kCrate = 1, kHero = 2, kZone = 3, kLift = 4 };

Bounds box_bounds(Vec3 half) {
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

BodyDesc body(ObjectId id, const PropertyMap& props, Vec3 position, Vec3 half) {
  BodyDesc desc;
  desc.object = id;
  desc.config = body_config_from_props(props);
  desc.world.position = position;
  desc.bounds = box_bounds(half);
  return desc;
}

std::unique_ptr<PhysicsWorld> world_with_ground() {
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  REQUIRE_FALSE(world->add_object(body(kGround, {{"collider", std::string("box")}}, {0, -0.5f, 0}, {20, 0.5f, 20})));
  return world;
}

void run(PhysicsWorld& world, int steps) {
  for (int i = 0; i < steps; ++i) world.step(kDt);
}

bool has_event(const std::vector<ContactEvent>& events, ObjectId self, ObjectId other, ContactPhase phase,
               bool trigger) {
  return std::any_of(events.begin(), events.end(), [&](const ContactEvent& e) {
    return e.self == self && e.other == other && e.phase == phase && e.trigger == trigger;
  });
}

}  // namespace

TEST_CASE("a dynamic box falls and rests on static ground") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}}, {0, 3, 0}, {0.5f, 0.5f, 0.5f})));
  run(*world, kSettleSteps);
  const auto pose = world->simulated_pose(kCrate);
  REQUIRE(pose.has_value());
  CHECK(pose->position.y == doctest::Approx(0.5f).epsilon(kPositionTolerance));
  CHECK(length(world->velocity(kCrate)) < kPositionTolerance);
}

TEST_CASE("contacts produce enter events for both bodies") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}}, {0, 1, 0}, {0.5f, 0.5f, 0.5f})));
  std::vector<ContactEvent> events;
  for (int i = 0; i < kSettleSteps; ++i) {
    world->step(kDt);
    for (const auto& e : world->drain_events()) events.push_back(e);
  }
  CHECK(has_event(events, kCrate, kGround, ContactPhase::Enter, false));
  CHECK(has_event(events, kGround, kCrate, ContactPhase::Enter, false));
}

TEST_CASE("a character lands, walks and jumps") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kHero, {{"body", std::string("character")}}, {0, 2, 0}, {0.4f, 1.0f, 0.4f})));
  run(*world, kSettleSteps);
  CHECK(world->character_grounded(kHero));
  CHECK(world->simulated_pose(kHero)->position.y == doctest::Approx(1.0f).epsilon(kPositionTolerance));

  world->set_character_move(kHero, {3, 0, 0});
  run(*world, static_cast<int>(1.0f / kDt));
  CHECK(world->simulated_pose(kHero)->position.x == doctest::Approx(3.0f).epsilon(0.1));

  world->set_character_move(kHero, {0, 0, 0});
  world->character_jump(kHero, 8.0f);
  run(*world, 10);
  CHECK_FALSE(world->character_grounded(kHero));
  CHECK(world->simulated_pose(kHero)->position.y > 1.5f);
}

TEST_CASE("trigger volumes report enter and exit") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kHero, {{"body", std::string("character")}}, {0, 1, 0}, {0.4f, 1.0f, 0.4f})));
  REQUIRE_FALSE(world->add_object(body(kZone, {{"trigger", true}}, {3, 1, 0}, {0.5f, 1.0f, 0.5f})));
  CHECK(world->is_trigger(kZone));
  world->set_target_pose(kZone, Transform{{3, 1, 0}, Quat::identity(), Vec3::one()});

  std::vector<ContactEvent> events;
  world->set_character_move(kHero, {4, 0, 0});
  for (int i = 0; i < kSettleSteps; ++i) {
    world->step(kDt);
    for (const auto& e : world->drain_events()) events.push_back(e);
  }
  CHECK(has_event(events, kZone, kHero, ContactPhase::Enter, true));
  CHECK(has_event(events, kZone, kHero, ContactPhase::Exit, true));
}

TEST_CASE("kinematic bodies follow their target pose") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kLift, {{"body", std::string("kinematic")}}, {0, 1, 0}, {1, 0.1f, 1})));
  world->set_target_pose(kLift, Transform{{0, 2, 0}, Quat::identity(), Vec3::one()});
  world->step(kDt);
  const auto hit = world->raycast(kAboveGround, kDown, kRayLength, kNoObject);
  REQUIRE(hit.has_value());
  CHECK(hit->object == kLift);
  CHECK(hit->point.y == doctest::Approx(2.1f).epsilon(kPositionTolerance));
}

TEST_CASE("raycasts hit, miss and ignore") {
  auto world = world_with_ground();
  const auto hit = world->raycast(kAboveGround, kDown, kRayLength, kNoObject);
  REQUIRE(hit.has_value());
  CHECK(hit->object == kGround);
  CHECK(hit->distance == doctest::Approx(5.0f).epsilon(kPositionTolerance));
  CHECK(hit->normal.y == doctest::Approx(1.0f).epsilon(kPositionTolerance));
  CHECK_FALSE(world->raycast(kAboveGround, kUp, kRayLength, kNoObject).has_value());
  CHECK_FALSE(world->raycast(kAboveGround, kDown, kRayLength, kGround).has_value());
}

TEST_CASE("removed objects disappear") {
  auto world = world_with_ground();
  world->remove_object(kGround);
  CHECK_FALSE(world->has_object(kGround));
  CHECK_FALSE(world->raycast(kAboveGround, kDown, kRayLength, kNoObject).has_value());
  CHECK(world->add_object(body(kCrate, {{"collider", std::string("mesh")}}, {0, 0, 0}, {1, 1, 1})).has_value());
}

TEST_CASE("triggers ignore static geometry") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kZone, {{"trigger", true}}, Vec3::zero(), kZoneHalfExtents)));
  world->set_target_pose(kZone, Transform{Vec3::zero(), Quat::identity(), Vec3::one()});
  run(*world, kOneStep);
  CHECK(world->drain_events().empty());
}

TEST_CASE("static bodies can be teleported") {
  auto world = world_with_ground();
  world->teleport(kGround, Transform{kFarBelow, Quat::identity(), Vec3::one()});
  CHECK_FALSE(world->raycast(kAboveGround, kDown, kRayLength, kNoObject).has_value());
}

TEST_CASE("several physics worlds can live at the same time") {
  auto first = world_with_ground();
  {
    auto second = world_with_ground();
    run(*second, kOneStep);
  }
  REQUIRE(first->raycast(kAboveGround, kDown, kRayLength, kNoObject).has_value());
  REQUIRE_FALSE(first->add_object(body(kCrate, {{"body", std::string("dynamic")}}, kDropHeight, kCrateHalfExtents)));
  run(*first, kOneStep);
}

TEST_CASE("a mesh collider is made only of triangles that name the mesh's own vertices") {
  constexpr std::uint32_t kNoSuchVertex = 4000000;
  PrimitiveDesc floor;
  floor.positions = {-5, 0, -5, 5, 0, -5, 5, 0, 5, -5, 0, 5};
  floor.normals = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
  floor.indices = {0, 2, 1, 0, 3, 2};
  PrimitiveDesc broken = floor;
  broken.indices = {0, 1, kNoSuchVertex};
  MeshDesc mesh;
  mesh.primitives = {broken};
  BodyDesc desc = body(kGround, {{"collider", std::string("mesh")}}, {0, 0, 0}, {5, 0.5f, 5});
  desc.mesh = &mesh;
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  const std::optional<std::string> refused = world->add_object(desc);
  REQUIRE(refused.has_value());
  CHECK(refused->find("no triangles") != std::string::npos);

  // The sound part of a mesh still makes ground to stand on.
  mesh.primitives = {broken, floor};
  REQUIRE_FALSE(world->add_object(desc));
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}}, kDropHeight, kCrateHalfExtents)));
  run(*world, kSettleSteps);
  CHECK(world->simulated_pose(kCrate)->position.y == doctest::Approx(kCrateHalfExtents.y).epsilon(kPositionTolerance));
}
