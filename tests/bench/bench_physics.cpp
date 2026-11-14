// The physics world's step: boxes falling onto the ground and onto each other. Each sample starts
// from a new world (made outside the measure), so that every sample times the same second of
// falling and landing, before anything settles and sleeps.

#include <memory>
#include <string>

#include "bench/bench_case.hpp"
#include "core/config.hpp"
#include "physics/physics_config.hpp"
#include "physics/physics_world.hpp"

namespace redgem::bench {

namespace {

constexpr int kBoxesPerSide = 10;
constexpr int kLayers = 4;
constexpr int kBodies = kBoxesPerSide * kBoxesPerSide * kLayers;
constexpr int kStepsPerSample = 60;  // one second at the fixed step
constexpr float kSpacing = 1.5f;     // meters between the middles of two boxes
constexpr float kFirstHeight = 2.0f;
constexpr float kBoxHalf = 0.5f;
constexpr float kGroundHalfWidth = 40.0f;
constexpr float kGroundHalfHeight = 0.5f;
constexpr double kBoxMass = 1.0;
constexpr ObjectId kGround = 0;

Bounds box_bounds(Vec3 half) {
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

void add_body(PhysicsWorld& world, ObjectId id, const PropertyMap& props, Vec3 position, Vec3 half) {
  BodyDesc desc;
  desc.object = id;
  desc.config = body_config_from_props(props);
  desc.world.position = position;
  desc.bounds = box_bounds(half);
  require(!world.add_object(desc), "adding a body");
}

Vec3 box_position(int index) {
  const int column = index % kBoxesPerSide;
  const int row = (index / kBoxesPerSide) % kBoxesPerSide;
  const int layer = index / (kBoxesPerSide * kBoxesPerSide);
  return {column * kSpacing, kFirstHeight + layer * kSpacing, row * kSpacing};
}

std::unique_ptr<PhysicsWorld> falling_boxes() {
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  add_body(*world, kGround, {{"collider", std::string("box")}}, {0.0f, -kGroundHalfHeight, 0.0f},
           {kGroundHalfWidth, kGroundHalfHeight, kGroundHalfWidth});
  const PropertyMap box = {{"body", std::string("dynamic")}, {"mass", kBoxMass}};
  for (int i = 0; i < kBodies; ++i)
    add_body(*world, kGround + 1 + i, box, box_position(i), {kBoxHalf, kBoxHalf, kBoxHalf});
  return world;
}

double step_sample() {
  const std::unique_ptr<PhysicsWorld> world = falling_boxes();
  return microseconds_each(kStepsPerSample, [&world] { world->step(config::kFixedDeltaSeconds); });
}

}  // namespace

BenchCase physics_case() {
  const std::string what = "one step of " + std::to_string(kBodies) + " falling boxes";
  return {"physics.step_bodies", what, [] { return Sampler(step_sample); }};
}

}  // namespace redgem::bench
