// A character that crouches: its capsule gets shorter with its feet kept on the ground, it walks
// under a low ceiling, cannot stand up there, and stands up once it is out. Fixed steps only.

#include <doctest/doctest.h>

#include <memory>
#include <optional>

#include "core/config.hpp"
#include "physics/physics_world.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr float kDt = config::kFixedDeltaSeconds;
constexpr int kSettleSteps = 120;
constexpr int kOneSecond = 60;
constexpr float kTolerance = 0.05f;

enum Ids : ObjectId { kGround = 0, kHero = 1, kCeiling = 2, kCrate = 3 };

constexpr float kRadius = 0.4f;
constexpr float kStanding = 2.0f;
constexpr float kCrouching = 1.0f;
constexpr Vec3 kHeroHalf{kRadius, kStanding / 2, kRadius};
constexpr Vec3 kStart{0.0f, kStanding / 2, 0.0f};
// A slab whose underside is 1.3 m up: a crouching character fits under it, a standing one does not.
constexpr float kCeilingUnderside = 1.3f;
constexpr Vec3 kCeilingHalf{1.5f, 0.1f, 2.0f};
constexpr Vec3 kCeilingCenter{3.5f, kCeilingUnderside + kCeilingHalf.y, 0.0f};
constexpr Vec3 kWalk{3.5f, 0.0f, 0.0f};  // a second of it ends under the slab's middle
constexpr Vec3 kStill{0.0f, 0.0f, 0.0f};

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

void run(PhysicsWorld& world, int steps) {
  for (int i = 0; i < steps; ++i) world.step(kDt);
}

struct Room {
  std::unique_ptr<PhysicsWorld> world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});

  Room() {
    const PropertyMap solid{{"collider", std::string("box")}};
    REQUIRE_FALSE(world->add_object(body(kGround, solid, {0, -0.5f, 0}, {20, 0.5f, 20})));
    REQUIRE_FALSE(world->add_object(body(kCeiling, solid, kCeilingCenter, kCeilingHalf)));
    REQUIRE_FALSE(world->add_object(body(kHero, {{"body", std::string("character")}}, kStart, kHeroHalf)));
    run(*world, kSettleSteps);
  }

  Bounds hero() const { return *world->body_bounds(kHero); }
  float x() const { return world->simulated_pose(kHero)->position.x; }

  void walk(Vec3 velocity) {
    world->set_character_move(kHero, velocity);
    run(*world, kOneSecond);
    world->set_character_move(kHero, kStill);
  }
};

}  // namespace

TEST_CASE("a crouching character keeps its feet on the ground and is as tall as asked") {
  Room room;
  REQUIRE(room.hero().max.y == doctest::Approx(kStanding).epsilon(kTolerance));
  CHECK(room.world->resize_character(kHero, kCrouching, kRadius));
  CHECK(room.hero().min.y == doctest::Approx(0.0f).epsilon(kTolerance));
  CHECK(room.hero().max.y == doctest::Approx(kCrouching).epsilon(kTolerance));
  run(*room.world, kSettleSteps);
  CHECK(room.world->character_grounded(kHero));
  CHECK(room.hero().max.y == doctest::Approx(kCrouching).epsilon(kTolerance));
}

TEST_CASE("a character crouches under a low ceiling, cannot stand there, and stands once out") {
  Room room;
  REQUIRE(room.world->resize_character(kHero, kCrouching, kRadius));
  room.walk(kWalk);
  REQUIRE(room.x() > kCeilingCenter.x - kCeilingHalf.x);  // under the slab

  CHECK_FALSE(room.world->resize_character(kHero, kStanding, kRadius));
  CHECK(room.hero().max.y < kCeilingUnderside);
  run(*room.world, kSettleSteps);
  CHECK(room.hero().max.y < kCeilingUnderside);  // the step after does not push it up either

  room.walk(kWalk);
  REQUIRE(room.x() > kCeilingCenter.x + kCeilingHalf.x + kRadius);  // out from under it
  CHECK(room.world->resize_character(kHero, kStanding, kRadius));
  CHECK(room.hero().max.y == doctest::Approx(kStanding).epsilon(kTolerance));
  CHECK(room.hero().min.y == doctest::Approx(0.0f).epsilon(kTolerance));
}

TEST_CASE("a resize changes nothing for what is not a character, a size it has, or a size no capsule has") {
  Room room;
  CHECK_FALSE(room.world->resize_character(kGround, kCrouching, kRadius));
  CHECK_FALSE(room.world->resize_character(kHero, kStanding, kRadius));  // the size it has
  CHECK_FALSE(room.world->resize_character(kHero, kRadius, kRadius));    // shorter than it is wide
  CHECK_FALSE(room.world->resize_character(kHero, kCrouching, 0.0f));
  CHECK(room.hero().max.y == doctest::Approx(kStanding).epsilon(kTolerance));
}

TEST_CASE("a shrinking character is never refused, even pressed against a wall") {
  Room room;
  const PropertyMap solid{{"collider", std::string("box")}};
  // A crate right against its side: growing wider would go into it, shrinking never does.
  REQUIRE_FALSE(room.world->add_object(body(kCrate, solid, {kRadius + 0.5f, 1.0f, 0.0f}, {0.5f, 1.0f, 0.5f})));
  CHECK(room.world->resize_character(kHero, kCrouching, kRadius / 2));
  CHECK_FALSE(room.world->resize_character(kHero, kStanding, kRadius * 2));
  room.world->remove_object(kCrate);
  CHECK(room.world->resize_character(kHero, kStanding, kRadius * 2));
}
