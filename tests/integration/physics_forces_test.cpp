// What scripts do to rigid bodies besides moving them: forces and torques, spin, mass, gravity,
// sleep, and switching between dynamic and kinematic.

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "core/config.hpp"
#include "physics/physics_config.hpp"
#include "physics/physics_world.hpp"

using namespace redgem;

namespace {

constexpr float kDt = config::kFixedDeltaSeconds;
constexpr int kOneSecond = 60;
constexpr int kSettleSteps = 120;
constexpr float kTolerance = 0.05f;
constexpr Vec3 kUnitHalf{0.5f, 0.5f, 0.5f};
constexpr Vec3 kAboveGround{0, 3, 0};

enum Ids : ObjectId { kGround = 0, kCrate = 1, kLift = 2, kCar = 3 };

Bounds box_bounds(Vec3 half) {
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

BodyDesc body(ObjectId id, const PropertyMap& props, Vec3 position) {
  BodyDesc desc;
  desc.object = id;
  desc.config = body_config_from_props(props);
  desc.world.position = position;
  desc.bounds = box_bounds(kUnitHalf);
  return desc;
}

std::unique_ptr<PhysicsWorld> world_with_ground() {
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  BodyDesc ground = body(kGround, {{"collider", std::string("box")}}, {0, -0.5f, 0});
  ground.bounds = box_bounds({40, 0.5f, 40});
  REQUIRE_FALSE(world->add_object(ground));
  return world;
}

// A crate floating in the air, gravity off: only what scripts do moves it.
std::unique_ptr<PhysicsWorld> world_with_floating_crate(double mass) {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}, {"mass", mass}}, kAboveGround)));
  world->set_gravity_scale(kCrate, 0.0f);
  return world;
}

void run(PhysicsWorld& world, int steps) {
  for (int i = 0; i < steps; ++i) world.step(kDt);
}

void push_for(PhysicsWorld& world, int steps, Vec3 force) {
  for (int i = 0; i < steps; ++i) {
    world.add_force(kCrate, force);
    world.step(kDt);
  }
}

}  // namespace

TEST_CASE("a force pushed every step accelerates a body by force over mass") {
  constexpr double kMass = 2.0;
  constexpr Vec3 kPush{10, 0, 0};
  auto world = world_with_floating_crate(kMass);
  CHECK(world->mass(kCrate) == doctest::Approx(kMass));
  push_for(*world, kOneSecond, kPush);
  CHECK(world->velocity(kCrate).x == doctest::Approx(5.0f).epsilon(kTolerance));  // 10 N / 2 kg for 1 s

  // A force lasts one step only: then the body coasts (slowing a little, as bodies do).
  const float pushed = world->velocity(kCrate).x;
  run(*world, kOneSecond);
  CHECK(world->velocity(kCrate).x <= pushed);
  CHECK(world->velocity(kCrate).x > pushed * kHalf);
}

TEST_CASE("a heavier body gets less from the same force, and turns as it did") {
  constexpr Vec3 kPush{10, 0, 0};
  auto world = world_with_floating_crate(1.0);
  world->set_mass(kCrate, 4.0f);
  CHECK(world->mass(kCrate) == doctest::Approx(4.0f));
  push_for(*world, kOneSecond, kPush);
  CHECK(world->velocity(kCrate).x == doctest::Approx(2.5f).epsilon(kTolerance));
}

TEST_CASE("a torque, or a force off the center, spins a body; its spin can be read and set") {
  auto world = world_with_floating_crate(1.0);
  for (int i = 0; i < kOneSecond; ++i) {
    world->add_torque(kCrate, {0, 1, 0});
    world->step(kDt);
  }
  CHECK(world->angular_velocity(kCrate).y > 1.0f);
  world->set_angular_velocity(kCrate, Vec3::zero());
  CHECK(length(world->angular_velocity(kCrate)) < kTolerance);

  world->add_force_at(kCrate, {0, 0, 10}, kAboveGround + Vec3{1, 0, 0});
  world->step(kDt);
  CHECK(world->angular_velocity(kCrate).y < -kTolerance);  // pushed +z at +x: it turns clockwise from above
  CHECK(world->velocity(kCrate).z > 0.0f);
}

TEST_CASE("gravity scale 0 floats, 1 falls") {
  auto world = world_with_floating_crate(1.0);
  CHECK(world->gravity_scale(kCrate) == doctest::Approx(0.0f));
  run(*world, kOneSecond);
  CHECK(world->simulated_pose(kCrate)->position.y == doctest::Approx(kAboveGround.y).epsilon(kTolerance));
  world->set_gravity_scale(kCrate, 1.0f);
  run(*world, kOneSecond);
  CHECK(world->simulated_pose(kCrate)->position.y < kAboveGround.y - 1.0f);
}

TEST_CASE("a dynamic body turned kinematic stops falling, and falls again once dynamic") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}}, kAboveGround)));
  REQUIRE(world->set_body_type(kCrate, BodyType::Kinematic));
  CHECK(world->body_type(kCrate) == BodyType::Kinematic);
  run(*world, kOneSecond);
  CHECK_FALSE(world->simulated_pose(kCrate).has_value());  // the scene says where it is now
  REQUIRE(world->set_body_type(kCrate, BodyType::Dynamic));
  run(*world, kSettleSteps);
  CHECK(world->simulated_pose(kCrate)->position.y == doctest::Approx(0.5f).epsilon(kTolerance));
}

TEST_CASE("a kinematic body made dynamic keeps the mass it was given") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kLift, {{"body", std::string("kinematic")}, {"mass", 3.0}}, kAboveGround)));
  REQUIRE(world->set_body_type(kLift, BodyType::Dynamic));
  CHECK(world->mass(kLift) == doctest::Approx(3.0f));
  run(*world, kSettleSteps);
  CHECK(world->simulated_pose(kLift)->position.y == doctest::Approx(0.5f).epsilon(kTolerance));
}

TEST_CASE("only dynamic and kinematic bodies switch between the two") {
  auto world = world_with_ground();
  CHECK_FALSE(world->set_body_type(kGround, BodyType::Dynamic));
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}}, kAboveGround)));
  CHECK_FALSE(world->set_body_type(kCrate, BodyType::Static));
  CHECK(world->body_type(kCrate) == BodyType::Dynamic);
}

TEST_CASE("a sleeping body hangs where it is until woken, and keeps touching what it lay on") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}}, {0, 0.5f, 0})));
  run(*world, kSettleSteps);
  world->drain_events();
  world->set_sleeping(kCrate, true);
  CHECK(world->sleeping(kCrate));
  run(*world, kOneSecond);
  CHECK(world->drain_events().empty());  // no exit from the ground it rests on

  world->teleport(kCrate, Transform{kAboveGround, Quat::identity(), Vec3::one()});
  world->set_sleeping(kCrate, true);
  run(*world, kOneSecond);
  CHECK(world->simulated_pose(kCrate)->position.y == doctest::Approx(kAboveGround.y).epsilon(kTolerance));
  world->set_sleeping(kCrate, false);
  CHECK_FALSE(world->sleeping(kCrate));
  run(*world, kSettleSteps);
  CHECK(world->simulated_pose(kCrate)->position.y == doctest::Approx(0.5f).epsilon(kTolerance));
}

TEST_CASE("static ground has no mass, no spin and never sleeps in a script's eyes") {
  auto world = world_with_ground();
  world->add_force(kGround, {0, 100, 0});
  CHECK(world->mass(kGround) == doctest::Approx(0.0f));
  CHECK(length(world->angular_velocity(kGround)) == doctest::Approx(0.0f));
  CHECK_FALSE(world->sleeping(kGround));
  CHECK(world->gravity_scale(kGround) == doctest::Approx(1.0f));
}

TEST_CASE("switching a body to the type it has changes nothing") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}}, kAboveGround)));
  run(*world, kOneSecond / 4);
  const float falling = world->velocity(kCrate).y;
  REQUIRE(falling < -1.0f);
  CHECK(world->set_body_type(kCrate, BodyType::Dynamic));
  CHECK(world->velocity(kCrate).y == doctest::Approx(falling));
}

TEST_CASE("a sleeping body stops touching the ground once the ground moves away") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kCrate, {{"body", std::string("dynamic")}}, {0, 0.5f, 0})));
  run(*world, kSettleSteps);
  world->drain_events();
  world->set_sleeping(kCrate, true);
  world->teleport(kGround, Transform{{0, -50, 0}, Quat::identity(), Vec3::one()});
  run(*world, 1);
  const std::vector<ContactEvent> events = world->drain_events();
  CHECK(std::any_of(events.begin(), events.end(), [](const ContactEvent& e) { return e.phase == ContactPhase::Exit; }));
  CHECK(world->sleeping(kCrate));  // nothing woke it: it hangs where it slept
}
