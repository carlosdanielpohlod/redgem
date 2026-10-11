#include <doctest/doctest.h>

#include <cmath>

#include "scene/particles.hpp"

using namespace redgem;

namespace {

constexpr int kBurstCount = 40;
constexpr float kStep = 1.0f / 60.0f;
constexpr float kLife = 1.0f;
constexpr float kLifeVariationRoom = 1.3f;  // a particle lives at most this many times the burst's life
constexpr float kNarrowSpread = 10.0f;      // degrees
constexpr float kWorldGravity = -20.0f;
constexpr int kTooMany = 10000;
constexpr int kLimit = 4096;

ParticleBurst sparks() {
  ParticleBurst burst;
  burst.position = {1.0f, 2.0f, 3.0f};
  burst.count = kBurstCount;
  burst.life = kLife;
  return burst;
}

}  // namespace

TEST_CASE("a burst starts where it was asked and flies apart") {
  ParticleSystem system;
  CHECK(system.burst(sparks()) == kBurstCount);
  REQUIRE(system.particles().size() == kBurstCount);
  CHECK(approx_equal(system.particles().front().position, Vec3{1.0f, 2.0f, 3.0f}, kEpsilon));
  system.update(kStep);
  float farthest = 0.0f;
  for (const Particle& particle : system.particles()) {
    farthest = std::max(farthest, length(particle.position - Vec3{1.0f, 2.0f, 3.0f}));
  }
  CHECK(farthest > 0.0f);
}

TEST_CASE("particles are gone when their life is over") {
  ParticleSystem system;
  system.burst(sparks());
  system.update(kLife * kHalf);
  CHECK_FALSE(system.particles().empty());
  system.update(kLife * kLifeVariationRoom);
  CHECK(system.particles().empty());
}

TEST_CASE("a narrow spread keeps particles near the direction") {
  ParticleSystem system;
  ParticleBurst jet = sparks();
  jet.direction = {0.0f, 0.0f, -2.0f};  // any length
  jet.spread_degrees = kNarrowSpread;
  system.burst(jet);
  const float least_along = std::cos(degrees_to_radians(kNarrowSpread)) - kEpsilon;
  for (const Particle& particle : system.particles()) {
    CHECK(dot(normalize(particle.velocity), Vec3{0.0f, 0.0f, -1.0f}) >= least_along);
  }
}

TEST_CASE("gravity pulls particles as much as the burst says: fully, not at all, or upwards") {
  for (const float share : {1.0f, 0.0f, -1.0f}) {
    ParticleSystem system;
    system.set_gravity({0.0f, kWorldGravity, 0.0f});
    ParticleBurst still = sparks();
    still.speed = 0.0f;
    still.gravity = share;
    system.burst(still);
    system.update(kStep);
    const float moved = system.particles().front().position.y - still.position.y;
    if (share > 0.0f) CHECK(moved < 0.0f);
    if (share == 0.0f) CHECK(moved == doctest::Approx(0.0f));
    if (share < 0.0f) CHECK(moved > 0.0f);
  }
}

TEST_CASE("particles fade and shrink at the end of their life") {
  ParticleSystem system;
  ParticleBurst one = sparks();
  one.count = 1;
  system.burst(one);
  const Particle fresh = system.particles().front();
  CHECK(fresh.alpha_now() == doctest::Approx(1.0f));
  CHECK(fresh.size_now() == doctest::Approx(fresh.size));
  Particle old = fresh;
  old.age = old.life * 0.9f;
  CHECK(old.alpha_now() < fresh.alpha_now());
  CHECK(old.size_now() < fresh.size_now());
}

TEST_CASE("the same bursts always give the same particles, and there is a limit to how many") {
  ParticleSystem first, second;
  first.burst(sparks());
  second.burst(sparks());
  first.update(kStep);
  second.update(kStep);
  for (std::size_t i = 0; i < first.particles().size(); ++i) {
    CHECK(approx_equal(first.particles()[i].position, second.particles()[i].position, 0.0f));
  }
  ParticleBurst flood = sparks();
  flood.count = kTooMany;
  CHECK(first.burst(flood) == kLimit - kBurstCount);
  CHECK(first.burst(flood) == 0);
  first.clear();
  CHECK(first.particles().empty());
}
