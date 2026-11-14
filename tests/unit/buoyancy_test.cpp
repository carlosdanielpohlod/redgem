#include <doctest/doctest.h>

#include <cmath>

#include "physics/buoyancy.hpp"

using namespace redgem;

namespace {

constexpr float kGravity = 20.0f;  // meters per second squared
constexpr float kMass = 4.0f;
constexpr float kHalfSize = 0.5f;
constexpr float kStep = 1.0f / 60.0f;
constexpr int kSettleSteps = 1200;  // twenty seconds
constexpr float kDepthTolerance = 0.01f;
constexpr float kLightDensity = 250.0f;
constexpr float kHeavyDensity = 2000.0f;

Bounds box_at(Vec3 center) { return {center - Vec3::one() * kHalfSize, center + Vec3::one() * kHalfSize, true}; }

ColumnWater calm(float height) {
  ColumnWater water{};
  for (auto& column : water) column = height;
  return water;
}

BuoyancyBody body_at(Vec3 center, float density) {
  BuoyancyBody body;
  body.bounds = box_at(center);
  body.mass = kMass;
  body.density = density;
  body.gravity = kGravity;
  return body;
}

Vec3 total_force(const std::vector<BuoyancyPush>& pushes) {
  Vec3 total = Vec3::zero();
  for (const auto& push : pushes) total = total + push.force;
  return total;
}

// Drops a box into calm water at height 0 and lets it bob, the way a physics step would.
float settled_center(float density) {
  BuoyancyBody body = body_at({0.0f, 2.0f, 0.0f}, density);
  for (int i = 0; i < kSettleSteps; ++i) {
    const Vec3 force = total_force(buoyancy_pushes(body, calm(0.0f))) - Vec3::up() * (kMass * kGravity);
    body.velocity = body.velocity + force * (kStep / kMass);
    const Vec3 moved = Vec3::up() * (body.velocity.y * kStep);
    body.bounds = {body.bounds.min + moved, body.bounds.max + moved, true};
  }
  return body.bounds.center().y;
}

}  // namespace

TEST_CASE("the submerged fraction of a span goes from none above the water to all of it below") {
  CHECK(submerged_fraction(1.0f, 2.0f, 0.0f) == 0.0f);
  CHECK(submerged_fraction(-1.0f, 1.0f, 0.0f) == doctest::Approx(0.5f));
  CHECK(submerged_fraction(-2.0f, -1.0f, 0.0f) == 1.0f);
  CHECK(submerged_fraction(1.0f, 1.0f, 2.0f) == 0.0f);  // a flat span displaces nothing
  CHECK(submerged_fraction(0.0f, 1.0f, NAN) == 0.0f);
}

TEST_CASE("water pushes up as hard as the weight of what is displaced, Archimedes' law") {
  const float weight = kMass * kGravity;
  CHECK(buoyant_force(kMass, kWaterDensity, kGravity, 1.0f).y == doctest::Approx(weight));
  CHECK(buoyant_force(kMass, kDefaultBodyDensity, kGravity, 1.0f).y == doctest::Approx(weight * 2.0f));
  const float resting = floating_fraction(kDefaultBodyDensity);
  CHECK(buoyant_force(kMass, kDefaultBodyDensity, kGravity, resting).y == doctest::Approx(weight));
  CHECK(buoyant_force(kMass, 0.0f, kGravity, 1.0f).y == 0.0f);
  CHECK(floating_fraction(kHeavyDensity) == 1.0f);
}

TEST_CASE("water drag opposes motion, harder vertically, and grows with how much is under") {
  const Vec3 drag = water_drag({2.0f, -2.0f, 0.0f}, kMass, 1.0f);
  CHECK(drag.x < 0.0f);
  CHECK(drag.y > 0.0f);
  CHECK(std::fabs(drag.y) > std::fabs(drag.x));
  CHECK(water_drag({0.0f, 1.0f, 0.0f}, kMass, kHalf).y ==
        doctest::Approx(water_drag({0.0f, 1.0f, 0.0f}, kMass, 1.0f).y * kHalf));
}

TEST_CASE("a body is sampled as a grid of columns over its footprint, pushed where each is wet") {
  const auto columns = buoyancy_columns(box_at(Vec3::zero()));
  CHECK(columns[0].x == doctest::Approx(-0.25f));
  CHECK(columns[0].z == doctest::Approx(-0.25f));
  CHECK(columns[kBuoyancyColumns - 1].x == doctest::Approx(0.25f));
  CHECK(columns[0].y == doctest::Approx(-kHalfSize));
  const auto pushes = buoyancy_pushes(body_at(Vec3::zero(), kDefaultBodyDensity), calm(0.0f));
  REQUIRE(pushes.size() == kBuoyancyColumns);
  CHECK(pushes[0].point.y == doctest::Approx(-0.25f));  // the middle of the wet half
  CHECK(total_force(pushes).y == doctest::Approx(kMass * kGravity));
}

TEST_CASE("only wet columns push, so a body half over the shore tips toward the dry side") {
  ColumnWater half{};
  half[0] = 0.0f;
  half[1] = 0.0f;
  const auto pushes = buoyancy_pushes(body_at(Vec3::zero(), kDefaultBodyDensity), half);
  REQUIRE(pushes.size() == 2);
  for (const auto& push : pushes) CHECK(push.point.x < 0.0f);
  CHECK(buoyancy_pushes(body_at(Vec3::zero(), kDefaultBodyDensity), ColumnWater{}).empty());
  CHECK(buoyancy_pushes(body_at({0.0f, 5.0f, 0.0f}, kDefaultBodyDensity), calm(0.0f)).empty());
}

TEST_CASE("bodies without a usable mass, density or box are not pushed") {
  BuoyancyBody body = body_at(Vec3::zero(), kDefaultBodyDensity);
  body.mass = 0.0f;
  CHECK_FALSE(buoyant(body));
  body = body_at(Vec3::zero(), -1.0f);
  CHECK_FALSE(buoyant(body));
  body = body_at(Vec3::zero(), kDefaultBodyDensity);
  body.bounds.valid = false;
  CHECK(buoyancy_pushes(body, calm(0.0f)).empty());
}

TEST_CASE("a floating body settles with the share of its height its density says under the water") {
  CHECK(settled_center(kDefaultBodyDensity) == doctest::Approx(0.0f).epsilon(kDepthTolerance));
  const float light_bottom = settled_center(kLightDensity) - kHalfSize;
  CHECK(-light_bottom == doctest::Approx(floating_fraction(kLightDensity)).epsilon(kDepthTolerance));
  CHECK(settled_center(kHeavyDensity) < -kHalfSize * 4.0f);  // sinks
}
