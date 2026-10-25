// Joints in the physics world: what each kind holds, and how long it lasts.

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <variant>

#include "core/config.hpp"
#include "physics/physics_config.hpp"
#include "physics/physics_world.hpp"

using namespace redgem;

namespace {

constexpr float kDt = config::kFixedDeltaSeconds;
constexpr int kOneSecond = 60;
constexpr int kTwoSeconds = kOneSecond * 2;
constexpr float kTolerance = 0.05f;
constexpr float kQuarterTurn = kPi * kHalf;
constexpr Vec3 kUnitHalf{0.5f, 0.5f, 0.5f};
constexpr Vec3 kDoorHalf{1.0f, 1.0f, 0.05f};
constexpr Vec3 kHinge{0, 1.5f, 0};
constexpr Vec3 kHook{0, 8, 0};

enum Ids : ObjectId { kGround = 0, kDoor = 1, kBall = 2, kCrate = 3, kHero = 4, kZone = 5 };

Bounds box_bounds(Vec3 half) {
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

BodyDesc body(ObjectId id, const PropertyMap& props, Vec3 position, Vec3 half = kUnitHalf) {
  BodyDesc desc;
  desc.object = id;
  desc.config = body_config_from_props(props);
  desc.world.position = position;
  desc.bounds = box_bounds(half);
  return desc;
}

PropertyMap dynamic(double mass = 1.0) { return {{"body", std::string("dynamic")}, {"mass", mass}}; }

std::unique_ptr<PhysicsWorld> world_with_ground() {
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  REQUIRE_FALSE(world->add_object(body(kGround, {{"collider", std::string("box")}}, {0, -0.5f, 0}, {40, 0.5f, 40})));
  return world;
}

void run(PhysicsWorld& world, int steps) {
  for (int i = 0; i < steps; ++i) world.step(kDt);
}

JointId joined(PhysicsWorld& world, const JointDesc& desc) {
  const std::variant<JointId, std::string> made = world.add_joint(desc);
  if (const auto* problem = std::get_if<std::string>(&made)) FAIL(*problem);
  return std::get<JointId>(made);
}

JointDesc joint(JointKind kind, ObjectId a, ObjectId b, Vec3 anchor) {
  JointDesc desc;
  desc.kind = kind;
  desc.a = a;
  desc.b = b;
  desc.anchor = anchor;
  return desc;
}

// A ball 3 m under the hook on a 3 m rope.
JointDesc rope(float length = 3.0f) {
  JointDesc desc = joint(JointKind::Distance, kBall, kNoObject, kHook - Vec3{0, length, 0});
  desc.anchor_b = kHook;
  desc.max_distance = length;
  return desc;
}

float height_of(const PhysicsWorld& world, ObjectId object) { return world.simulated_pose(object)->position.y; }

}  // namespace

TEST_CASE("a hinged door driven by its motor swings and stops at its limit") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kDoor, dynamic(), kHinge + Vec3{1, 0, 0}, kDoorHalf)));
  JointDesc desc = joint(JointKind::Hinge, kDoor, kNoObject, kHinge);
  desc.axis = Vec3::up();
  desc.limits = std::make_pair(0.0f, kQuarterTurn);
  desc.motor_speed = kPi;  // half a turn a second: past the limit within a second
  desc.motor_torque = 100.0f;
  const JointId hinge = joined(*world, desc);
  run(*world, kTwoSeconds);
  CHECK(world->hinge_angle(hinge) == doctest::Approx(kQuarterTurn).epsilon(kTolerance));
  const Vec3 door = world->simulated_pose(kDoor)->position;
  CHECK(door.x == doctest::Approx(0.0f).epsilon(kTolerance));
  CHECK(door.z == doctest::Approx(-1.0f).epsilon(kTolerance));     // swung a quarter turn about +y
  CHECK(door.y == doctest::Approx(kHinge.y).epsilon(kTolerance));  // the hinge holds it up

  world->set_hinge_motor(hinge, -kPi, 100.0f);
  run(*world, kTwoSeconds);
  CHECK(world->hinge_angle(hinge) == doctest::Approx(0.0f).epsilon(kTolerance));
}

TEST_CASE("a ball on a distance joint hangs from its hook instead of falling") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(
      body(kBall, {{"body", std::string("dynamic")}, {"collider", std::string("sphere")}}, kHook - Vec3{1, 3, 0})));
  JointDesc desc = rope();
  desc.anchor = kHook - Vec3{1, 3, 0};
  desc.max_distance = length(desc.anchor - kHook);
  joined(*world, desc);
  run(*world, kOneSecond * 5);
  const Vec3 ball = world->simulated_pose(kBall)->position;
  CHECK(length(ball - kHook) == doctest::Approx(desc.max_distance).epsilon(kTolerance));
  CHECK(ball.y < kHook.y - 2.5f);  // it swung down under the hook
}

TEST_CASE("a slider moves along its axis only, between its limits") {
  auto world = world_with_ground();
  const Vec3 start{0, 3, 0};
  REQUIRE_FALSE(world->add_object(body(kCrate, dynamic(), start)));
  JointDesc desc = joint(JointKind::Slider, kCrate, kNoObject, start);
  desc.axis = Vec3::right();
  desc.limits = std::make_pair(0.0f, 2.0f);
  joined(*world, desc);
  for (int i = 0; i < kTwoSeconds; ++i) {
    world->add_force(kCrate, {20, 0, 5});
    world->step(kDt);
  }
  const Vec3 crate = world->simulated_pose(kCrate)->position;
  CHECK(crate.x == doctest::Approx(2.0f).epsilon(kTolerance));
  CHECK(crate.y == doctest::Approx(start.y).epsilon(kTolerance));
  CHECK(crate.z == doctest::Approx(start.z).epsilon(kTolerance));
}

TEST_CASE("a point joint lets a body swing every way around it") {
  auto world = world_with_ground();
  const Vec3 pivot{0, 5, 0};
  REQUIRE_FALSE(world->add_object(body(kCrate, dynamic(), pivot + Vec3{1, 0, 0})));
  joined(*world, joint(JointKind::Point, kCrate, kNoObject, pivot));
  run(*world, kOneSecond / 2);
  const Vec3 crate = world->simulated_pose(kCrate)->position;
  CHECK(crate.y < pivot.y - kHalf);
  CHECK(length(crate - pivot) == doctest::Approx(1.0f).epsilon(kTolerance));
}

TEST_CASE("a fixed joint holds a body where it is, or two bodies as they are") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kCrate, dynamic(), {0, 5, 0})));
  REQUIRE_FALSE(world->add_object(body(kBall, dynamic(), {1, 5, 0})));
  joined(*world, joint(JointKind::Fixed, kCrate, kNoObject, {0, 5, 0}));
  joined(*world, joint(JointKind::Fixed, kBall, kCrate, {0.5f, 5, 0}));
  run(*world, kOneSecond);
  CHECK(height_of(*world, kCrate) == doctest::Approx(5.0f).epsilon(kTolerance));
  CHECK(height_of(*world, kBall) == doctest::Approx(5.0f).epsilon(kTolerance));
}

TEST_CASE("a joint goes with either of its objects, and the other end moves freely again") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kBall, dynamic(), rope().anchor)));
  REQUIRE_FALSE(world->add_object(body(kCrate, dynamic(), {3, 5, 0})));
  const JointId hanging = joined(*world, rope());
  const JointId glued = joined(*world, joint(JointKind::Fixed, kCrate, kBall, {1.5f, 5, 0}));
  CHECK(hanging != glued);
  world->remove_object(kBall);
  CHECK_FALSE(world->has_joint(hanging));
  CHECK_FALSE(world->has_joint(glued));
  CHECK_FALSE(world->joint_broken(glued));
  run(*world, kTwoSeconds);
  CHECK(height_of(*world, kCrate) == doctest::Approx(0.5f).epsilon(kTolerance));
}

TEST_CASE("a removed or disabled joint lets go; ids are never given twice") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kBall, dynamic(), rope().anchor)));
  const JointId first = joined(*world, rope());
  world->set_joint_enabled(first, false);
  CHECK_FALSE(world->joint_enabled(first));
  run(*world, kTwoSeconds);
  CHECK(height_of(*world, kBall) == doctest::Approx(0.5f).epsilon(kTolerance));
  world->remove_joint(first);
  CHECK_FALSE(world->has_joint(first));
  CHECK(joined(*world, rope(4.5f)) > first);
}

TEST_CASE("a joint pulled harder than its break force breaks") {
  constexpr double kHeavy = 10.0;  // 200 N of weight
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kBall, dynamic(kHeavy), rope().anchor)));
  JointDesc weak = rope();
  weak.break_force = 50.0f;
  const JointId joint = joined(*world, weak);
  CHECK(world->joint_break_force(joint) == doctest::Approx(50.0f));
  run(*world, kOneSecond);
  CHECK_FALSE(world->has_joint(joint));
  CHECK(world->joint_broken(joint));
  run(*world, kOneSecond);
  CHECK(height_of(*world, kBall) == doctest::Approx(0.5f).epsilon(kTolerance));
}

TEST_CASE("a strong enough joint holds") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kBall, dynamic(), rope().anchor)));
  JointDesc strong = rope();
  strong.break_force = 1000.0f;  // 20 N of weight
  const JointId joint = joined(*world, strong);
  run(*world, kTwoSeconds);
  CHECK(world->has_joint(joint));
  CHECK(height_of(*world, kBall) == doctest::Approx(rope().anchor.y).epsilon(kTolerance));
}

TEST_CASE("joints hold rigid bodies only, a dynamic one at least") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kHero, {{"body", std::string("character")}}, {0, 1, 0})));
  REQUIRE_FALSE(world->add_object(body(kZone, {{"trigger", true}}, {3, 1, 0})));
  REQUIRE_FALSE(world->add_object(body(kCrate, dynamic(), {5, 1, 0})));
  const auto refused = [&](ObjectId a, ObjectId b) {
    return std::holds_alternative<std::string>(world->add_joint(joint(JointKind::Point, a, b, Vec3::zero())));
  };
  CHECK(refused(kHero, kNoObject));
  CHECK(refused(kZone, kNoObject));
  CHECK(refused(kCrate, kZone));
  CHECK(refused(kGround, kNoObject));
  CHECK(refused(kCrate, kCrate));
  CHECK_FALSE(refused(kCrate, kGround));
}

TEST_CASE("a joint twisted harder than its break force breaks, even with its points together") {
  auto world = world_with_ground();
  const Vec3 at{0, 5, 0};
  REQUIRE_FALSE(world->add_object(body(kCrate, dynamic(), at)));
  JointDesc glue = joint(JointKind::Fixed, kCrate, kNoObject, at);
  glue.break_force = 50.0f;
  const JointId joint = joined(*world, glue);
  world->set_gravity_scale(kCrate, 0.0f);
  run(*world, kOneSecond);
  CHECK(world->has_joint(joint));
  world->add_torque(kCrate, {0, 1000, 0});
  world->step(kDt);
  CHECK(world->joint_broken(joint));
}

TEST_CASE("a disabled joint does not break, whatever it held before") {
  auto world = world_with_ground();
  REQUIRE_FALSE(world->add_object(body(kBall, dynamic(), rope().anchor)));
  const JointId joint = joined(*world, rope());
  run(*world, kOneSecond);  // it holds the ball's weight
  world->set_joint_enabled(joint, false);
  world->set_joint_break_force(joint, 1.0f);
  run(*world, kOneSecond);
  CHECK(world->has_joint(joint));
  CHECK_FALSE(world->joint_broken(joint));
}
