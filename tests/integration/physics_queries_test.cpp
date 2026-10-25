// Collision layers and queries in the physics world: what meets what, and what rays, moving shapes
// and shapes in place find.

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/config.hpp"
#include "physics/physics_config.hpp"
#include "physics/physics_world.hpp"

using namespace redgem;

namespace {

constexpr float kDt = config::kFixedDeltaSeconds;
constexpr float kTolerance = 0.05f;
constexpr int kOneSecond = 60;
constexpr Vec3 kUnitHalf{0.5f, 0.5f, 0.5f};
constexpr Vec3 kEast{1, 0, 0};
constexpr Vec3 kDown{0, -1, 0};
constexpr float kRayLength = 50.0f;
constexpr Vec3 kHighAbove{0, 10, 0};

enum Ids : ObjectId { kGround = 0, kWall = 1, kPickup = 2, kBullet = 3, kEnemy = 4, kZone = 5, kHero = 6 };

// default, player, enemy, bullet, pickup: bullets only meet enemies and the default layer.
CollisionLayers shooter_layers() {
  return collision_layers_from_json(nlohmann::json::parse(R"({
    "layers": ["default", "player", "enemy", "bullet", "pickup"],
    "collides": {"bullet": ["enemy", "default"]}
  })"))
      .layers;
}

Bounds box_bounds(Vec3 half) {
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

BodyDesc body(ObjectId id, PropertyMap props, Vec3 position, Vec3 half = kUnitHalf) {
  BodyDesc desc;
  desc.object = id;
  desc.config = body_config_from_props(props);
  desc.world.position = position;
  desc.bounds = box_bounds(half);
  return desc;
}

PropertyMap on_layer(const std::string& body, const std::string& layer) { return {{"body", body}, {"layer", layer}}; }

std::unique_ptr<PhysicsWorld> shooter_world() {
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f}, shooter_layers());
  REQUIRE_FALSE(world->add_object(body(kGround, {{"collider", std::string("box")}}, {0, -0.5f, 0}, {40, 0.5f, 40})));
  return world;
}

void run(PhysicsWorld& world, int steps) {
  for (int i = 0; i < steps; ++i) world.step(kDt);
}

std::vector<ObjectId> objects_of(const std::vector<RaycastHit>& hits) {
  std::vector<ObjectId> objects;
  for (const RaycastHit& hit : hits) objects.push_back(hit.object);
  return objects;
}

LayerMask mask_of(const PhysicsWorld& world, const std::string& layer) {
  return layer_bit(*world.layers().find(layer));
}

QueryShape sphere(float radius) {
  QueryShape shape;
  shape.radius = radius;
  return shape;
}

}  // namespace

TEST_CASE("a bullet passes through a pickup its layer does not meet, and stops at a wall") {
  auto world = shooter_world();
  REQUIRE_FALSE(world->add_object(body(kPickup, on_layer("static", "pickup"), {4, 1, 0})));
  REQUIRE_FALSE(world->add_object(body(kWall, {{"collider", std::string("box")}}, {10, 1, 0}, {0.5f, 2, 2})));
  REQUIRE_FALSE(world->add_object(body(kBullet, on_layer("dynamic", "bullet"), {0, 1, 0}, {0.1f, 0.1f, 0.1f})));
  CHECK(world->layer(kBullet) == *world->layers().find("bullet"));
  world->set_gravity_scale(kBullet, 0.0f);
  world->set_velocity(kBullet, {20, 0, 0});
  run(*world, kOneSecond);
  const float bullet_x = world->simulated_pose(kBullet)->position.x;
  CHECK(bullet_x > 4.5f);  // past the pickup
  CHECK(bullet_x < 9.5f);  // short of the wall's far side: the wall stopped it
}

TEST_CASE("a layer changed while the game runs changes what a body meets") {
  auto world = shooter_world();
  REQUIRE_FALSE(world->add_object(body(kPickup, on_layer("static", "pickup"), {4, 1, 0})));
  REQUIRE_FALSE(world->add_object(body(kBullet, on_layer("dynamic", "default"), {0, 1, 0}, {0.1f, 0.1f, 0.1f})));
  world->set_gravity_scale(kBullet, 0.0f);
  world->set_velocity(kBullet, {20, 0, 0});
  run(*world, kOneSecond);
  CHECK(world->simulated_pose(kBullet)->position.x < 4.0f);  // stopped by the pickup

  world->teleport(kBullet, Transform{{0, 1, 0}, Quat::identity(), Vec3::one()});
  world->set_layer(kBullet, *world->layers().find("bullet"));
  world->set_velocity(kBullet, {20, 0, 0});
  run(*world, kOneSecond);
  CHECK(world->simulated_pose(kBullet)->position.x > 4.5f);
}

TEST_CASE("a trigger notices only what its layer meets") {
  auto world = shooter_world();
  PropertyMap zone{{"trigger", true}, {"layer", std::string("pickup")}};
  REQUIRE_FALSE(world->add_object(body(kZone, zone, {0, 1, 0}, {2, 2, 2})));
  world->set_target_pose(kZone, Transform{{0, 1, 0}, Quat::identity(), Vec3::one()});
  REQUIRE_FALSE(world->add_object(body(kBullet, on_layer("kinematic", "bullet"), {0, 1, 0})));
  REQUIRE_FALSE(world->add_object(body(kEnemy, on_layer("kinematic", "enemy"), {1, 1, 0})));
  run(*world, 1);
  const std::vector<ContactEvent> events = world->drain_events();
  REQUIRE(events.size() == 1);
  CHECK(events.front().other == kEnemy);
}

TEST_CASE("a character walks through what its layer does not meet") {
  auto world = shooter_world();
  REQUIRE_FALSE(world->add_object(body(kHero, on_layer("character", "bullet"), {0, 1, 0}, {0.4f, 1, 0.4f})));
  REQUIRE_FALSE(world->add_object(body(kPickup, on_layer("static", "pickup"), {2, 1, 0}, {0.5f, 2, 2})));
  world->set_character_move(kHero, {4, 0, 0});
  run(*world, kOneSecond);
  CHECK(world->simulated_pose(kHero)->position.x > 3.0f);
}

TEST_CASE("a raycast looks at the layers it is given and through every object it ignores") {
  auto world = shooter_world();
  REQUIRE_FALSE(world->add_object(body(kPickup, on_layer("static", "pickup"), {0, 4, 0})));
  REQUIRE_FALSE(world->add_object(body(kEnemy, on_layer("static", "enemy"), {0, 2, 0})));
  CHECK(world->raycast(kHighAbove, kDown, kRayLength, kNoObject)->object == kPickup);
  const LayerMask enemies_and_ground = mask_of(*world, "enemy") | mask_of(*world, "default");
  CHECK(world->raycast(kHighAbove, kDown, kRayLength, {enemies_and_ground, {}})->object == kEnemy);
  CHECK(world->raycast(kHighAbove, kDown, kRayLength, {kAllLayers, {kPickup, kEnemy}})->object == kGround);
  CHECK_FALSE(world->raycast(kHighAbove, kDown, kRayLength, {mask_of(*world, "player"), {}}).has_value());
}

TEST_CASE("raycast_all finds everything along the ray, nearest first, each object once") {
  auto world = shooter_world();
  REQUIRE_FALSE(world->add_object(body(kPickup, on_layer("static", "pickup"), {0, 4, 0})));
  REQUIRE_FALSE(world->add_object(body(kEnemy, on_layer("static", "enemy"), {0, 2, 0})));
  const std::vector<RaycastHit> hits = world->raycast_all(kHighAbove, kDown, kRayLength, {});
  CHECK(objects_of(hits) == std::vector<ObjectId>{kPickup, kEnemy, kGround});
  CHECK(hits[0].distance == doctest::Approx(5.5f).epsilon(kTolerance));
  CHECK(hits[1].point.y == doctest::Approx(2.5f).epsilon(kTolerance));
  CHECK(hits[2].normal.y == doctest::Approx(1.0f).epsilon(kTolerance));
  CHECK(objects_of(world->raycast_all(kHighAbove, kDown, kRayLength, {mask_of(*world, "enemy"), {}})) ==
        std::vector<ObjectId>{kEnemy});
}

TEST_CASE("a query reports at most kMaxQueryHits hits, the nearest ones") {
  constexpr ObjectId kFirstBox = 10;
  constexpr int kBoxes = static_cast<int>(config::kMaxQueryHits) + 6;
  constexpr float kSpacing = 2.0f;
  auto world = shooter_world();
  for (int i = 0; i < kBoxes; ++i) {
    const Vec3 at{kSpacing * static_cast<float>(i + 1), 1, 0};
    REQUIRE_FALSE(world->add_object(body(kFirstBox + i, {{"collider", std::string("box")}}, at)));
  }
  const std::vector<RaycastHit> hits = world->raycast_all({0, 1, 0}, kEast, kSpacing * kBoxes * kTwo, {});
  REQUIRE(hits.size() == config::kMaxQueryHits);
  CHECK(hits.front().object == kFirstBox);
  CHECK(hits.back().object == kFirstBox + config::kMaxQueryHits - 1);
}

TEST_CASE("a sphere moving along a line touches the first thing in its way") {
  constexpr float kRadius = 0.5f;
  auto world = shooter_world();
  REQUIRE_FALSE(world->add_object(body(kPickup, on_layer("static", "pickup"), {3, 1, 0})));
  REQUIRE_FALSE(world->add_object(body(kWall, {{"collider", std::string("box")}}, {8, 1, 0})));
  const std::optional<RaycastHit> hit = world->shape_cast(sphere(kRadius), {0, 1, 0}, {20, 1, 0}, {});
  REQUIRE(hit.has_value());
  CHECK(hit->object == kPickup);
  CHECK(hit->distance == doctest::Approx(2.0f).epsilon(kTolerance));  // 3 - 0.5 (box) - 0.5 (sphere)
  CHECK(hit->point.x == doctest::Approx(2.5f).epsilon(kTolerance));
  CHECK(hit->normal.x == doctest::Approx(-1.0f).epsilon(kTolerance));

  const QueryFilter walls{kAllLayers, {kPickup}};
  CHECK(world->shape_cast(sphere(kRadius), {0, 1, 0}, {20, 1, 0}, walls)->object == kWall);
  CHECK_FALSE(world->shape_cast(sphere(kRadius), {0, 1, 0}, {1, 1, 0}, {}).has_value());
}

TEST_CASE("boxes and capsules can be cast as well") {
  auto world = shooter_world();
  REQUIRE_FALSE(world->add_object(body(kWall, {{"collider", std::string("box")}}, {6, 1, 0})));
  QueryShape box;
  box.kind = QueryShapeKind::Box;
  box.half_extents = {1, 0.5f, 0.5f};
  CHECK(world->shape_cast(box, {0, 1, 0}, {10, 1, 0}, {})->distance == doctest::Approx(4.5f).epsilon(kTolerance));
  QueryShape capsule;
  capsule.kind = QueryShapeKind::Capsule;
  capsule.radius = 0.25f;
  capsule.height = 1.5f;
  CHECK(world->shape_cast(capsule, {0, 1, 0}, {10, 1, 0}, {})->distance == doctest::Approx(5.25f).epsilon(kTolerance));
}

TEST_CASE("an overlap finds what a shape in place touches, on the layers it is given") {
  auto world = shooter_world();
  REQUIRE_FALSE(world->add_object(body(kPickup, on_layer("static", "pickup"), {1, 1, 0})));
  REQUIRE_FALSE(world->add_object(body(kEnemy, on_layer("kinematic", "enemy"), {-2, 1, 0})));
  REQUIRE_FALSE(world->add_object(body(kWall, {{"collider", std::string("box")}}, {9, 1, 0})));
  const std::vector<RaycastHit> near = world->overlap(sphere(2.0f), {0, 1, 0}, {});
  CHECK(objects_of(near) == std::vector<ObjectId>{kPickup, kGround, kEnemy});
  CHECK(objects_of(world->overlap(sphere(2.0f), {0, 1, 0}, {mask_of(*world, "enemy"), {}})) ==
        std::vector<ObjectId>{kEnemy});
  CHECK(objects_of(world->overlap(sphere(2.0f), {0, 1, 0}, {kAllLayers, {kGround, kPickup}})) ==
        std::vector<ObjectId>{kEnemy});
  QueryShape box;
  box.kind = QueryShapeKind::Box;
  box.half_extents = {0.5f, 0.5f, 0.5f};
  CHECK(world->overlap(box, {9, 3, 0}, {}).empty());
}

TEST_CASE("a sleeping body put on a layer that meets nothing leaves the ground it lay on") {
  auto world = create_jolt_physics_world(
      {0.0f, config::kGravityY, 0.0f},
      collision_layers_from_json(nlohmann::json::parse(R"({"layers": ["ghost"], "collides": {"ghost": []}})")).layers);
  REQUIRE_FALSE(world->add_object(body(kGround, {{"collider", std::string("box")}}, {0, -0.5f, 0}, {40, 0.5f, 40})));
  REQUIRE_FALSE(world->add_object(body(kBullet, on_layer("dynamic", "default"), {0, 0.5f, 0})));
  run(*world, kOneSecond);
  world->drain_events();
  world->set_sleeping(kBullet, true);
  run(*world, kOneSecond);
  CHECK(world->drain_events().empty());  // still lying there

  world->set_layer(kBullet, *world->layers().find("ghost"));
  run(*world, kOneSecond);
  bool left = false;
  for (const ContactEvent& event : world->drain_events()) left = left || event.phase == ContactPhase::Exit;
  CHECK(left);
  CHECK(world->simulated_pose(kBullet)->position.y < 0.0f);  // fell through
}
