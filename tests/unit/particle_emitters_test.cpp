#include <doctest/doctest.h>

#include <cmath>
#include <limits>

#include "scene/particle_emitters.hpp"

using namespace redgem;

namespace {
constexpr float kRate = 4.0f;
constexpr float kQuarterSecond = 0.25f;
constexpr float kLongLife = 100.0f;
constexpr float kTravel = 8.0f;

Scene emitter_scene() {
  SceneDesc desc;
  NodeDesc object;
  object.name = "Emitter";
  desc.nodes = {object};
  return Scene::from_desc(desc);
}
ParticleBurst still_births() {
  ParticleBurst burst;
  burst.speed = 0.0f;
  burst.gravity = 0.0f;
  burst.life = kLongLife;
  return burst;
}
}  // namespace

TEST_CASE("continuous emitters accumulate fractional births and stopping leaves old particles alive") {
  Scene scene = emitter_scene();
  ParticleSystem particles;
  ParticleEmitters emitters;
  const int id = emitters.create(scene, scene.handle(0), still_births(), kRate);
  REQUIRE(id != kNoEmitter);
  emitters.advance(kQuarterSecond * kHalf, scene, particles);
  CHECK(particles.particles().empty());
  emitters.advance(kQuarterSecond * kHalf, scene, particles);
  CHECK(particles.particles().size() == 1);
  CHECK(emitters.active(scene, id));
  CHECK(emitters.stop(id));
  CHECK_FALSE(emitters.stop(id));
  CHECK_FALSE(emitters.active(scene, id));
  emitters.advance(1.0f, scene, particles);
  CHECK(particles.particles().size() == 1);
}

TEST_CASE("continuous emitter births follow object movement without moving particles already emitted") {
  Scene scene = emitter_scene();
  ParticleSystem particles;
  ParticleEmitters emitters;
  emitters.create(scene, scene.handle(0), still_births(), kRate);
  emitters.advance(kQuarterSecond, scene, particles);
  scene.at(0).local.position.x = kTravel;
  emitters.advance(1.0f, scene, particles);
  REQUIRE(particles.particles().size() == 5);
  CHECK(particles.particles().front().position.x == doctest::Approx(0));
  for (std::size_t i = 1; i < particles.particles().size(); ++i)
    CHECK(particles.particles()[i].position.x == doctest::Approx(static_cast<float>(i) * kTravel / kRate));
}

TEST_CASE("continuous emitter local offsets and directions include the object's hierarchy") {
  SceneDesc desc;
  NodeDesc parent;
  parent.name = "Parent";
  parent.children = {1};
  parent.local.position = {kTravel, 0, 0};
  parent.local.rotation = quat_from_axis_angle(Vec3::forward(), kPi * kHalf);
  parent.local.scale = {2, 2, 2};
  NodeDesc child;
  child.name = "Emitter";
  child.parent = 0;
  child.local.position = Vec3::right();
  desc.nodes = {parent, child};
  Scene scene = Scene::from_desc(desc);
  auto settings = still_births();
  settings.position = Vec3::right();
  settings.direction = Vec3::right();
  settings.spread_degrees = 0;
  settings.speed = 1;
  ParticleSystem particles;
  ParticleEmitters emitters;
  emitters.create(scene, scene.handle(1), settings, kRate);
  emitters.advance(kQuarterSecond, scene, particles);
  REQUIRE(particles.particles().size() == 1);
  const auto& particle = particles.particles().front();
  const Transform world = scene.world_transform(1);
  CHECK(approx_equal(particle.position, world.position + rotate(world.rotation, Vec3{2, 0, 0}), kEpsilon));
  CHECK(approx_equal(normalize(particle.velocity), rotate(world.rotation, Vec3::right()), kEpsilon));
}

TEST_CASE("continuous emitters end on destroyed objects or scene replacement and never reuse handles") {
  Scene scene = emitter_scene();
  ParticleSystem particles;
  ParticleEmitters emitters;
  const int first = emitters.create(scene, scene.handle(0), still_births(), kRate);
  scene.request_destroy(0);
  scene.finalize_destroy(0);
  CHECK_FALSE(emitters.active(scene, first));
  emitters.advance(1.0f, scene, particles);
  CHECK(particles.particles().empty());
  scene = emitter_scene();
  const int second = emitters.create(scene, scene.handle(0), still_births(), kRate);
  CHECK(second != first);
  CHECK_FALSE(emitters.stop(first));
  CHECK(emitters.active(scene, second));
  scene = emitter_scene();
  CHECK_FALSE(emitters.active(scene, second));
  const int third = emitters.create(scene, scene.handle(0), still_births(), kRate);
  emitters.clear();
  const int fourth = emitters.create(scene, scene.handle(0), still_births(), kRate);
  CHECK(fourth != third);
  CHECK_FALSE(emitters.stop(third));
  CHECK(emitters.active(scene, fourth));
}

TEST_CASE("continuous emitters drop missed births at the particle cap and resume without a backlog") {
  Scene scene = emitter_scene();
  ParticleSystem particles;
  auto fill = still_births();
  fill.count = static_cast<int>(kMaxParticles);
  particles.burst(fill);
  ParticleEmitters emitters;
  emitters.create(scene, scene.handle(0), still_births(), kRate);
  emitters.advance(kLongLife, scene, particles);
  CHECK(particles.particles().size() == kMaxParticles);
  particles.clear();
  emitters.advance(kQuarterSecond, scene, particles);
  CHECK(particles.particles().size() == 1);
}

TEST_CASE("continuous emitter limits reject invalid rates and cap even an enormous time step") {
  Scene scene = emitter_scene();
  ParticleSystem particles;
  ParticleEmitters emitters;
  for (const float invalid : {0.0f, -1.0f, kMaxEmitterRate + 1, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()})
    CHECK(emitters.create(scene, scene.handle(0), still_births(), invalid) == kNoEmitter);
  CHECK(emitters.create(scene, kNoHandle, still_births(), kRate) == kNoEmitter);
  for (std::size_t i = 0; i < kMaxParticleEmitters; ++i)
    REQUIRE(emitters.create(scene, scene.handle(0), still_births(), kMaxEmitterRate) != kNoEmitter);
  CHECK(emitters.create(scene, scene.handle(0), still_births(), kRate) == kNoEmitter);
  emitters.advance(std::numeric_limits<float>::max(), scene, particles);
  CHECK(particles.particles().size() == kMaxParticles);
}

TEST_CASE("paused continuous emitters emit nothing and do not leave a trail across paused movement") {
  Scene scene = emitter_scene();
  ParticleSystem particles;
  ParticleEmitters emitters;
  emitters.create(scene, scene.handle(0), still_births(), kRate);
  scene.at(0).local.position.x = kTravel;
  for (const float paused : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN()})
    emitters.advance(paused, scene, particles);
  CHECK(particles.particles().empty());
  emitters.advance(1.0f, scene, particles);
  REQUIRE(particles.particles().size() == static_cast<std::size_t>(kRate));
  for (const auto& particle : particles.particles()) CHECK(particle.position.x == doctest::Approx(kTravel));
}
