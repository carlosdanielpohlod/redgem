// Local steering among others: when two discs meet, the velocity a character takes to keep clear
// of the others (sharing the avoidance with those that steer too), and two walking at each other
// that pass and both get where they go.

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "navigation/nav_steering.hpp"

using namespace redgem;

namespace {

constexpr float kRadius = 0.4f;
constexpr float kSpeed = 2.0f;
constexpr float kTolerance = 1e-4f;
constexpr float kStepSeconds = 1.0f / 60.0f;
constexpr int kMostSteps = 60 * 20;  // 20 s
constexpr float kArrived = 0.2f;
constexpr float kCrossing = 5.0f;  // meters from the middle each starts at

SteeringAgent walker(Vec3 position, Vec3 goal) {
  return {position, Vec3::zero(), normalize(goal - position) * kSpeed, kRadius, kSpeed};
}

// The velocity toward a goal, slowing down to stop on it.
Vec3 toward(Vec3 position, Vec3 goal) {
  const Vec3 away = goal - position;
  return length(away) < kArrived ? Vec3::zero() : normalize(away) * kSpeed;
}

// Two that walk to each other's place: one steps, then the other, as a frame of a game does.
struct Crossing {
  SteeringAgent one;
  SteeringAgent other;
  Vec3 one_goal;
  Vec3 other_goal;
  float closest = std::numeric_limits<float>::infinity();

  void step() {
    one.preferred = toward(one.position, one_goal);
    other.preferred = toward(other.position, other_goal);
    const Vec3 one_velocity = steer(one, {{other.position, other.velocity, other.radius, true}});
    const Vec3 other_velocity = steer(other, {{one.position, one.velocity, one.radius, true}});
    one.velocity = one_velocity;
    other.velocity = other_velocity;
    one.position = one.position + one_velocity * kStepSeconds;
    other.position = other.position + other_velocity * kStepSeconds;
    closest = std::fmin(closest, length(one.position - other.position));
  }
  bool arrived() const {
    return length(one.position - one_goal) < kArrived && length(other.position - other_goal) < kArrived;
  }
};

}  // namespace

TEST_CASE("time to collision: discs coming at each other, passing by, moving apart, touching") {
  CHECK(time_to_collision({10, 0, 0}, {2, 0, 0}, 1.0f) == doctest::Approx(4.5f));
  CHECK(std::isinf(time_to_collision({10, 0, 5}, {2, 0, 0}, 1.0f)));  // passes 5 m beside it
  CHECK(std::isinf(time_to_collision({10, 0, 0}, {-2, 0, 0}, 1.0f)));
  CHECK(std::isinf(time_to_collision({10, 0, 0}, {0, 0, 0}, 1.0f)));
  CHECK(time_to_collision({0.5f, 0, 0}, {0, 0, 0}, 1.0f) == 0.0f);
}

TEST_CASE("with nobody near, a character walks as it wants, flat, no faster than its fastest") {
  SteeringAgent agent = walker({0, 0, 0}, {10, 0, 0});
  CHECK(approx_equal(steer(agent, {}), Vec3{kSpeed, 0, 0}, kTolerance));
  agent.preferred = {kSpeed * 3, 1, 0};
  CHECK(approx_equal(steer(agent, {}), Vec3{kSpeed, 0, 0}, kTolerance));
  const SteeringObstacle far{{100, 0, 0}, Vec3::zero(), kRadius, false};
  CHECK(out_of_reach(agent, far));
  CHECK(approx_equal(steer(agent, {far}), Vec3{kSpeed, 0, 0}, kTolerance));
}

TEST_CASE("a character turns aside from someone standing in its way, and still goes on") {
  const SteeringAgent agent = walker({0, 0, 0}, {10, 0, 0});
  const SteeringObstacle standing{{2, 0, 0}, Vec3::zero(), kRadius, false};
  CHECK_FALSE(out_of_reach(agent, standing));
  const Vec3 velocity = steer(agent, {standing});
  CHECK(velocity.x > 0.0f);                   // on toward its goal
  CHECK(std::fabs(velocity.z) > kTolerance);  // but aside
  CHECK(velocity.y == 0.0f);
  const float hit = time_to_collision(standing.position - agent.position, velocity, kRadius * 2);
  CHECK(hit > time_to_collision(standing.position - agent.position, agent.preferred, kRadius * 2));
}

TEST_CASE("two face to face both keep to their right") {
  SteeringAgent east = walker({-2, 0, 0}, {10, 0, 0});
  SteeringAgent west = walker({2, 0, 0}, {-10, 0, 0});
  east.velocity = east.preferred;
  west.velocity = west.preferred;
  const Vec3 east_turn = steer(east, {{west.position, west.velocity, kRadius, true}});
  const Vec3 west_turn = steer(west, {{east.position, east.velocity, kRadius, true}});
  CHECK(east_turn.z > 0.0f);  // facing +x, the right is +z (looking down -z, it is +x)
  CHECK(west_turn.z < 0.0f);
}

TEST_CASE("a character keeps clear of a body coming across its way, which does not steer") {
  const SteeringAgent agent = walker({0, 0, 0}, {10, 0, 0});
  const SteeringObstacle crate{{3, 0, -3}, {0, 0, kSpeed}, kRadius * 2, false};
  const Vec3 velocity = steer(agent, {crate});
  const float reach = kRadius * 3;
  CHECK(time_to_collision(crate.position - agent.position, velocity - crate.velocity, reach) >
        time_to_collision(crate.position - agent.position, agent.preferred - crate.velocity, reach));
}

TEST_CASE("a velocity that may not be taken is not, and standing still always may") {
  const SteeringAgent agent = walker({0, 0, 0}, {10, 0, 0});
  const VelocityCheck no_east = [](Vec3 velocity) { return velocity.x <= 0.0f; };
  CHECK(steer(agent, {}, no_east).x <= 0.0f);
  const VelocityCheck nothing = [](Vec3) { return false; };
  CHECK(approx_equal(steer(agent, {}, nothing), Vec3::zero(), kTolerance));
}

TEST_CASE("two walking at each other across a corridor pass without touching, and both arrive") {
  Crossing crossing{walker({-kCrossing, 0, 0}, {kCrossing, 0, 0}),
                    walker({kCrossing, 0, 0}, {-kCrossing, 0, 0}),
                    {kCrossing, 0, 0},
                    {-kCrossing, 0, 0}};
  for (int step = 0; step < kMostSteps && !crossing.arrived(); ++step) crossing.step();
  CHECK(crossing.arrived());
  CHECK(crossing.closest >= kRadius * 2 - kTolerance);
}
