#include <doctest/doctest.h>

#include <cmath>
#include <limits>

#include "scene/water_interactions.hpp"

using namespace redgem;

namespace {

constexpr float kStep = 0.2f;  // seconds, longer than the pair's impulse cooldown
constexpr float kBodyHalf = 0.25f;
constexpr Vec3 kAbove{0.0f, 2.0f, 0.0f};
constexpr Vec3 kBelow{0.0f, -2.0f, 0.0f};
constexpr Vec3 kFarPond{20.0f, 0.0f, 0.0f};
constexpr float kWakeStep = 0.3f;     // meters, longer than the wake-spacing threshold
constexpr int kBoundedImpulses = 20;  // more than the shader packet can retain

PrimitiveDesc flat_quad(float left, float right, int material) {
  PrimitiveDesc primitive;
  primitive.material = material;
  primitive.positions = {left, 0.0f, -5.0f, right, 0.0f, -5.0f, right, 0.0f, 5.0f, left, 0.0f, 5.0f};
  primitive.normals = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  primitive.indices = {0, 1, 2, 0, 2, 3};
  return primitive;
}

SceneDesc pond_scene() {
  SceneDesc desc;
  MaterialDesc water;
  water.water.enabled = true;
  desc.materials = {water, MaterialDesc{}};
  desc.meshes = {MeshDesc{"Pond", {flat_quad(-5.0f, 5.0f, 0)}, {}}};
  NodeDesc pond;
  pond.name = "Pond";
  pond.mesh = 0;
  NodeDesc body;
  body.name = "Body";
  desc.nodes = {pond, body};
  return desc;
}

WaterBodySample sample(const Scene& scene, Vec3 center, ObjectId id = 1) {
  const Vec3 half = Vec3::one() * kBodyHalf;
  return {scene.handle(id), Bounds{center - half, center + half, true}, Vec3::zero()};
}

struct Pond {
  SceneDesc desc = pond_scene();
  Scene scene = Scene::from_desc(desc);
  WaterInteractions water;
  Pond() { water.reset(desc); }
  void at(Vec3 point, float delta = kStep) { water.step(scene, desc, {sample(scene, point)}, delta); }
  void fall() {
    at(kAbove, 0.0f);
    at(kBelow);
  }
};

}  // namespace

TEST_CASE("water contacts sweep fast bodies across the whole plane without a water collider") {
  Pond pond;
  pond.fall();
  REQUIRE(pond.water.snapshot().count == 1);
  const auto& ripple = pond.water.snapshot().ripples[0];
  CHECK(ripple.surface == pond.scene.handle(0));
  CHECK(ripple.primitive == 0);
  CHECK(ripple.origin.y == doctest::Approx(0.0f));
  CHECK(ripple.age == 0.0f);
  CHECK(ripple.strength <= kWaterRippleMaxStrength);
  CHECK(ripple.strength > 0.0f);
}

TEST_CASE("water exits and spaced surface motion emit bounded impulses, resting bodies do not") {
  Pond pond;
  pond.fall();
  pond.at(kAbove);
  CHECK(pond.water.snapshot().count == 2);
  pond.at({0.0f, 0.0f, 0.0f});
  const auto entering = pond.water.snapshot().count;
  pond.at({0.0f, 0.0f, 0.0f});
  CHECK(pond.water.snapshot().count == entering);
  pond.at({kWakeStep, 0.0f, 0.0f});
  CHECK(pond.water.snapshot().count == entering + 1);
  pond.at({kWakeStep * 2, 0.0f, 0.0f}, kWaterImpulseCooldown * kHalf);
  CHECK(pond.water.snapshot().count == entering + 1);
}

TEST_CASE("water footprint queries exclude dry primitives and gaps inside an overall mesh bound") {
  SceneDesc desc = pond_scene();
  desc.meshes[0].primitives = {flat_quad(-5.0f, -2.0f, 0), flat_quad(2.0f, 5.0f, 1)};
  const Scene scene = Scene::from_desc(desc);
  WaterInteractions water;
  water.reset(desc);
  for (const float x : {0.0f, 3.0f, 20.0f}) {
    water.forget(scene.handle(1));
    water.step(scene, desc, {sample(scene, {x, kAbove.y, 0.0f})}, 0.0f);
    water.step(scene, desc, {sample(scene, {x, kBelow.y, 0.0f})}, kStep);
    CHECK(water.snapshot().count == 0);
  }
  water.forget(scene.handle(1));
  water.step(scene, desc, {sample(scene, {-3.0f, kAbove.y, 0.0f})}, 0.0f);
  water.step(scene, desc, {sample(scene, {-3.0f, kBelow.y, 0.0f})}, kStep);
  REQUIRE(water.snapshot().count == 1);
  CHECK(water.snapshot().ripples[0].primitive == 0);
}

TEST_CASE("water triangles with a shared mesh retain their own surface owner") {
  SceneDesc desc = pond_scene();
  NodeDesc other = desc.nodes[0];
  other.local.position = kFarPond;
  desc.nodes.push_back(other);
  const Scene scene = Scene::from_desc(desc);
  WaterInteractions water;
  water.reset(desc);
  water.step(scene, desc, {sample(scene, kAbove + kFarPond)}, 0.0f);
  water.step(scene, desc, {sample(scene, kBelow + kFarPond)}, kStep);
  REQUIRE(water.snapshot().count == 1);
  CHECK(water.snapshot().ripples[0].surface == scene.handle(2));
}

TEST_CASE("water geometry caches unchanged surfaces and rebuilds after a water transform changes") {
  SceneDesc desc = pond_scene();
  Scene scene = Scene::from_desc(desc);
  WaterGeometryCache geometry;
  const auto* first = geometry.refresh(scene, desc)[0].triangles.data();
  CHECK(geometry.refresh(scene, desc)[0].triangles.data() == first);
  scene.set_world_position(0, kFarPond);
  const auto& moved = geometry.refresh(scene, desc);
  REQUIRE(moved.size() == 1);
  CHECK_FALSE(water_contact_point(moved[0], sample(scene, Vec3::zero()).bounds));
  CHECK(water_contact_point(moved[0], sample(scene, kFarPond).bounds).has_value());
}

TEST_CASE("water rejects tilted surfaces, invalid samples and nonfinite time") {
  Pond pond;
  auto bad = sample(pond.scene, kAbove);
  bad.velocity.x = std::numeric_limits<float>::infinity();
  pond.water.step(pond.scene, pond.desc, {bad}, 0.0f);
  pond.at(kBelow);
  CHECK(pond.water.snapshot().count == 0);
  const double time = pond.water.snapshot().time;
  pond.at(kAbove, std::numeric_limits<float>::quiet_NaN());
  CHECK(pond.water.snapshot().time == time);
  pond.scene.set_world_rotation(0, quat_from_axis_angle(Vec3::right(), kHalf));
  pond.fall();
  CHECK(pond.water.snapshot().count == 0);
}

TEST_CASE("water clocks pause, impulses expire, and world reset discards old surface and body state") {
  Pond pond;
  pond.fall();
  const auto before = pond.water.snapshot();
  pond.at(kAbove, 0.0f);
  CHECK(pond.water.snapshot().time == before.time);
  CHECK(pond.water.snapshot().ripples[0].age == before.ripples[0].age);
  pond.water.step(pond.scene, pond.desc, {}, kWaterRippleLifetime);
  CHECK(pond.water.snapshot().count == 0);
  pond.water.reset(pond.desc);
  CHECK(pond.water.snapshot().time == 0.0);
  pond.at(kBelow);
  CHECK(pond.water.snapshot().count == 0);
}

TEST_CASE("water script teleports restart sampling instead of sweeping through the pond") {
  Pond pond;
  pond.at(kAbove, 0.0f);
  pond.water.forget(pond.scene.handle(1));
  pond.at(kBelow, 0.0f);
  pond.at(kBelow);
  CHECK(pond.water.snapshot().count == 0);
}

TEST_CASE("water impulses remain bounded and resting surface jitter never emits a wake") {
  Pond pond;
  pond.at(Vec3::zero(), 0.0f);
  for (int i = 0; i < kBoundedImpulses; ++i) {
    const float sign = i % 2 == 0 ? 1.0f : -1.0f;
    pond.at({0.0f, sign * kWaterContactSkin * kHalf, 0.0f});
  }
  CHECK(pond.water.snapshot().count == 0);
  pond.water.reset(pond.desc);
  pond.at(kAbove, 0.0f);
  for (int i = 0; i < kBoundedImpulses; ++i) pond.at(i % 2 == 0 ? kBelow : kAbove, kWaterImpulseCooldown);
  CHECK(pond.water.snapshot().count == kMaxWaterRipples);
  for (std::size_t i = 0; i < pond.water.snapshot().count; ++i) {
    const auto& ripple = pond.water.snapshot().ripples[i];
    CHECK(std::isfinite(ripple.strength));
    CHECK(ripple.strength <= kWaterRippleMaxStrength);
  }
}

TEST_CASE("water body samples cannot follow a destroyed object into a reused slot") {
  Pond pond;
  const ObjectId first = pond.scene.clone(1).front();
  const ObjectHandle stale = pond.scene.handle(first);
  pond.water.step(pond.scene, pond.desc, {sample(pond.scene, kAbove, first)}, 0.0f);
  pond.scene.request_destroy(first);
  pond.scene.finalize_destroy(first);
  const ObjectId reused = pond.scene.clone(1).front();
  REQUIRE(reused == first);
  CHECK(pond.scene.handle(reused) != stale);
  pond.water.step(pond.scene, pond.desc, {sample(pond.scene, kBelow, reused)}, kStep);
  CHECK(pond.water.snapshot().count == 0);
}

TEST_CASE("living bodies visiting more water pairs than the contact budget can continue creating waves") {
  constexpr int kPonds = 65;
  constexpr int kVisitors = 16;  // 1040 distinct live pairs, greater than kMaxWaterContacts
  SceneDesc desc = pond_scene();
  std::vector<ObjectId> ponds{0};
  std::vector<ObjectId> bodies{1};
  for (int i = 1; i < kPonds; ++i) {
    NodeDesc pond = desc.nodes[0];
    pond.local.position = kFarPond * static_cast<float>(i);
    ponds.push_back(static_cast<ObjectId>(desc.nodes.size()));
    desc.nodes.push_back(pond);
  }
  for (int i = 1; i < kVisitors; ++i) {
    bodies.push_back(static_cast<ObjectId>(desc.nodes.size()));
    desc.nodes.push_back(desc.nodes[1]);
  }
  const Scene scene = Scene::from_desc(desc);
  WaterInteractions water;
  water.reset(desc);
  for (const ObjectId body : bodies) {
    for (const ObjectId pond : ponds) {
      const Vec3 offset = scene.world_transform(pond).position;
      water.step(scene, desc, {sample(scene, kAbove + offset, body)}, kStep);
      water.step(scene, desc, {sample(scene, kBelow + offset, body)}, kStep);
    }
  }
  REQUIRE(water.snapshot().count > 0);
  const auto& latest = water.snapshot().ripples[water.snapshot().count - 1];
  CHECK(latest.serial >= static_cast<std::uint64_t>(kPonds * kVisitors));
  CHECK(latest.surface == scene.handle(ponds.back()));
}

TEST_CASE("water cache keys and geometry apply the same bounded set of candidate objects") {
  SceneDesc desc = pond_scene();
  desc.nodes[0].local.rotation = quat_from_axis_angle(Vec3::right(), kHalf);
  for (std::size_t i = 1; i < kMaxWaterSurfaceObjects; ++i) desc.nodes.push_back(desc.nodes[0]);
  NodeDesc beyond_budget = desc.nodes[0];
  beyond_budget.local.rotation = Quat::identity();
  desc.nodes.push_back(beyond_budget);
  Scene scene = Scene::from_desc(desc);
  WaterGeometryCache geometry;
  CHECK(geometry.refresh(scene, desc).empty());
  scene.set_world_position(static_cast<ObjectId>(desc.nodes.size() - 1), kFarPond);
  CHECK(geometry.refresh(scene, desc).empty());
}
