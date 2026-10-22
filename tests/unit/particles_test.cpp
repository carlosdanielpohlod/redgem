#include <doctest/doctest.h>

#include <cmath>
#include <limits>

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
constexpr float kSpin = 90.0f;  // degrees per second

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
  CHECK(fresh.size_now() == doctest::Approx(fresh.look.size.keys[0]));
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

TEST_CASE("particles with a picture share it, start turned any which way and keep turning") {
  ParticleSystem system;
  ParticleBurst puffs = sparks();
  puffs.image = "smoke";
  puffs.spin_degrees = kSpin;
  system.burst(puffs);
  ParticleBurst stars = sparks();
  stars.image = "spark";
  system.burst(stars);
  system.burst(puffs);
  system.burst(sparks());  // plain dots
  CHECK(system.images() == std::vector<std::string>{"smoke", "spark"});
  const std::vector<Particle>& particles = system.particles();
  CHECK(particles.front().look.image == 0);
  CHECK(particles[kBurstCount].look.image == 1);
  CHECK(particles[kBurstCount * 2].look.image == 0);
  CHECK(particles.back().look.image == kNoParticleImage);
  CHECK(particles.back().angle == doctest::Approx(0.0f));
  CHECK(particles[0].angle != doctest::Approx(particles[1].angle));
  bool clockwise = false, counterclockwise = false;
  for (int i = 0; i < kBurstCount; ++i) {
    clockwise = clockwise || particles[static_cast<std::size_t>(i)].spin < 0.0f;
    counterclockwise = counterclockwise || particles[static_cast<std::size_t>(i)].spin > 0.0f;
  }
  CHECK(clockwise);
  CHECK(counterclockwise);
  const float before = particles.front().angle;
  system.update(kStep);
  CHECK(system.particles().front().angle != doctest::Approx(before));
  CHECK(system.particles()[kBurstCount].angle == doctest::Approx(particles[kBurstCount].angle));  // no spin asked
}

TEST_CASE("a burst's cone is built around its direction, or straight up when it has none") {
  constexpr float kLong = 5.0f;
  constexpr float kRightAngleSpread = 90.0f;
  const BurstCone cone = burst_cone({kLong, 0.0f, 0.0f}, kRightAngleSpread);
  CHECK(cone.axis.x == doctest::Approx(1.0f));
  CHECK(length(cone.side) == doctest::Approx(1.0f));
  CHECK(length(cone.other) == doctest::Approx(1.0f));
  CHECK(dot(cone.axis, cone.side) == doctest::Approx(0.0f));
  CHECK(dot(cone.axis, cone.other) == doctest::Approx(0.0f));
  CHECK(dot(cone.side, cone.other) == doctest::Approx(0.0f));
  CHECK(cone.widest == doctest::Approx(0.0f));  // the cosine of a right angle

  CHECK(burst_cone(Vec3::zero(), kRightAngleSpread).axis.y == doctest::Approx(1.0f));
  // A spread is at most every direction.
  constexpr float kTooWide = 720.0f;
  CHECK(burst_cone(Vec3::up(), kTooWide).widest == doctest::Approx(-1.0f));
}

TEST_CASE("a heading goes from the cone's axis to its edge, all the way around") {
  constexpr float kRightAngleSpread = 90.0f;
  constexpr float kHalfWayAround = 0.5f;
  const BurstCone cone = burst_cone(Vec3::up(), kRightAngleSpread);
  CHECK(heading_in(cone, 0.0f, 0.0f).y == doctest::Approx(1.0f));  // along the axis
  const Vec3 edge = heading_in(cone, 1.0f, 0.0f);
  CHECK(edge.y == doctest::Approx(0.0f));
  CHECK(length(edge) == doctest::Approx(1.0f));
  const Vec3 opposite = heading_in(cone, 1.0f, kHalfWayAround);
  CHECK(dot(edge, opposite) == doctest::Approx(-1.0f));
}

TEST_CASE("a value over life goes through its keys, evenly spread from birth to end") {
  constexpr float kAQuarter = 0.25f;
  const OverLife<float> one = steady(2.0f);
  CHECK(one.at(0.0f) == doctest::Approx(2.0f));
  CHECK(one.at(1.0f) == doctest::Approx(2.0f));
  const OverLife<float> two{{1.0f, 3.0f}, 2};
  CHECK(two.at(kHalf) == doctest::Approx(2.0f));
  const OverLife<float> four{{0.0f, 3.0f, 3.0f, 0.0f}, 4};  // fades in, holds, fades out
  CHECK(four.at(0.0f) == doctest::Approx(0.0f));
  CHECK(four.at(1.0f / 6.0f) == doctest::Approx(1.5f));
  CHECK(four.at(kHalf) == doctest::Approx(3.0f));
  CHECK(four.at(1.0f) == doctest::Approx(0.0f));
  CHECK(four.at(2.0f) == doctest::Approx(0.0f));  // past the end: the last key
  const OverLife<Vec3> colors{{Vec3::one(), Vec3::zero()}, 2};
  CHECK(approx_equal(colors.at(kAQuarter), Vec3::one() * (1.0f - kAQuarter), kEpsilon));
}

TEST_CASE("a single size and alpha hold for half the life, then shrink and fade as particles always did") {
  ParticleSystem system;
  ParticleBurst one = sparks();
  one.count = 1;
  system.burst(one);
  Particle particle = system.particles().front();
  const float size = particle.size_now();
  particle.age = particle.life * 0.4f;
  CHECK(particle.size_now() == doctest::Approx(size));
  CHECK(particle.alpha_now() == doctest::Approx(1.0f));
  particle.age = particle.life * 0.75f;
  CHECK(particle.size_now() == doctest::Approx(size * (1.0f + kEndSizeShare) / 2.0f));
  CHECK(particle.alpha_now() == doctest::Approx(kHalf));
  particle.age = particle.life;
  CHECK(particle.size_now() == doctest::Approx(size * kEndSizeShare));
  CHECK(particle.alpha_now() == doctest::Approx(0.0f));
}

TEST_CASE("each particle of a burst is a little bigger or smaller, all through its life alike") {
  ParticleSystem system;
  ParticleBurst growing = sparks();
  growing.look.size = OverLife<float>{{1.0f, 4.0f}, 2};
  system.burst(growing);
  for (const Particle& particle : system.particles()) {
    CHECK(particle.look.size.keys[1] == doctest::Approx(particle.look.size.keys[0] * 4.0f));
  }
  CHECK(system.particles()[0].look.size.keys[0] != doctest::Approx(system.particles()[1].look.size.keys[0]));
}

TEST_CASE("a flipbook shows its cells over the particle's life, or round and round at its rate") {
  constexpr float kLife = 2.0f;
  const Flipbook once{4, 2, 0.0f};  // 8 cells
  CHECK(flipbook_frame(once, 0.0f, kLife) == 0);
  CHECK(flipbook_frame(once, kLife * kHalf, kLife) == 4);
  CHECK(flipbook_frame(once, kLife * 0.99f, kLife) == 7);
  CHECK(flipbook_frame(once, kLife, kLife) == 7);  // the end shows the last one
  CHECK(flipbook_frame(once, 0.0f, 0.0f) == 7);    // a life of nothing is over
  const Flipbook looping{4, 2, 10.0f};             // ten a second
  CHECK(flipbook_frame(looping, 0.05f, kLife) == 0);
  CHECK(flipbook_frame(looping, 0.35f, kLife) == 3);
  CHECK(flipbook_frame(looping, 0.85f, kLife) == 0);    // round again after the eighth
  CHECK(flipbook_frame(Flipbook{}, 1.0f, kLife) == 0);  // a single picture
}

TEST_CASE("a flipbook played very fast for very long still shows one of its cells") {
  constexpr float kLongAge = 1.0e9f;  // seconds: more cells go by than an int can count
  const Flipbook fastest{4, 2, std::numeric_limits<float>::max()};
  const int frame = flipbook_frame(fastest, kLongAge, kLongAge);
  CHECK(frame >= 0);
  CHECK(frame < fastest.frames());
}

TEST_CASE("glowing particles shine with their own light, even when asked to be lit") {
  ParticleLook look;
  look.lit = true;
  CHECK(look.takes_light());
  look.glow = true;
  CHECK_FALSE(look.takes_light());
}

TEST_CASE("particle shadow casting is opt-in and carried from the burst into every particle") {
  ParticleSystem system;
  ParticleBurst burst = sparks();
  CHECK_FALSE(burst.look.cast_shadow);
  burst.look.cast_shadow = true;
  system.burst(burst);
  REQUIRE_FALSE(system.particles().empty());
  for (const auto& particle : system.particles()) CHECK(particle.look.cast_shadow);
}
