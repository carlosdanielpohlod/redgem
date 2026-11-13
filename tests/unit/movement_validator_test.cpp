// Movement validation: how fast and how far the host believes the others' objects move.

#include <doctest/doctest.h>

#include <string>

#include "net/movement_validator.hpp"

using redgem::Vec3;
using redgem::net::kCorrectionSeconds;
using redgem::net::kJitterSeconds;
using redgem::net::MovementRules;
using redgem::net::MovementValidator;
using redgem::net::MovementVerdict;

namespace {

constexpr int kPeer = 2;
constexpr int kOtherPeer = 3;
constexpr const char* kHero = "hero-2";
constexpr float kMaxSpeed = 10.0f;  // meters a second
constexpr float kTeleport = 5.0f;   // meters a pose
constexpr float kTolerance = 0.2f;
constexpr float kTick = 0.05f;                  // seconds between poses: twenty a second
constexpr float kFairStep = kMaxSpeed * kTick;  // what the top speed covers in a tick
constexpr int kTicks = 100;
constexpr float kSpeedHack = 3.0f;     // times the top speed
constexpr float kJitterFactor = 1.1f;  // a little over the top speed: within the tolerance
constexpr int kKickAfter = 3;
constexpr float kNear = 0.001f;
constexpr int kBunchedPoses = 4;  // arriving at once after a pause as long as they took
constexpr float kFarAway = 3.0f;  // times the teleport distance
constexpr float kStallSeconds = 2.0f;

MovementRules rules(float speed, float teleport, int kick_after = 0) {
  MovementRules made;
  made.max_speed = speed;
  made.teleport_distance = teleport;
  made.tolerance = kTolerance;
  made.kick_after = kick_after;
  return made;
}

MovementValidator validating(const MovementRules& given) {
  MovementValidator validator;
  validator.configure(given);
  return validator;
}

Vec3 along_x(float meters) { return {meters, 0.0f, 0.0f}; }

// Moves the hero `step` meters every tick from where it is believed to be, for `ticks` ticks: how
// many of its poses were not believed.
int refused_while_moving(MovementValidator& validator, float step, int ticks) {
  float x = 0.0f;
  int refused = 0;
  validator.check(kPeer, kHero, along_x(x));
  for (int i = 0; i < ticks; ++i) {
    validator.advance(kTick);
    const MovementVerdict verdict = validator.check(kPeer, kHero, along_x(x + step));
    refused += verdict.valid ? 0 : 1;
    x = verdict.position.x;
  }
  return refused;
}

}  // namespace

TEST_CASE("with no limits nothing is checked, as before") {
  MovementValidator validator;
  CHECK_FALSE(validator.enabled());
  validator.configure(MovementRules{});
  CHECK_FALSE(validator.enabled());
}

TEST_CASE("movement at the top speed is always believed") {
  MovementValidator validator = validating(rules(kMaxSpeed, kTeleport));
  CHECK(refused_while_moving(validator, kFairStep, kTicks) == 0);
  CHECK(validator.violations(kPeer) == 0);
}

TEST_CASE("the tolerance absorbs a little over the top speed, and poses that arrive unevenly") {
  MovementValidator validator = validating(rules(kMaxSpeed, kTeleport));
  CHECK(refused_while_moving(validator, kFairStep * kJitterFactor, kTicks) == 0);
  // Four poses at once after a pause: what the pause earned pays for them.
  MovementValidator bunched = validating(rules(kMaxSpeed, kTeleport));
  bunched.check(kPeer, kHero, along_x(0.0f));
  bunched.advance(kTick * static_cast<float>(kBunchedPoses));
  for (int i = 1; i <= kBunchedPoses; ++i)
    CHECK(bunched.check(kPeer, kHero, along_x(kFairStep * static_cast<float>(i))).valid);
}

TEST_CASE("a speed hack is cut down to what the top speed allows, and counted") {
  MovementValidator validator = validating(rules(kMaxSpeed, kTeleport));
  CHECK(refused_while_moving(validator, kFairStep * kSpeedHack, kTicks) > 0);
  CHECK(validator.violations(kPeer) > 0);
  // However long it goes on, it gets no further than the top speed (and its burst) take it.
  MovementValidator measured = validating(rules(kMaxSpeed, kTeleport));
  measured.check(kPeer, kHero, along_x(0.0f));
  float x = 0.0f;
  for (int i = 0; i < kTicks; ++i) {
    measured.advance(kTick);
    x = measured.check(kPeer, kHero, along_x(x + kFairStep * kSpeedHack)).position.x;
  }
  const float most = kMaxSpeed * (1.0f + kTolerance) * (kTick * kTicks + kJitterSeconds);
  CHECK(x <= most + kNear);
}

TEST_CASE("a move too fast is undone whole when the game says so") {
  MovementRules undoing = rules(kMaxSpeed, 0.0f);
  undoing.undo = true;
  MovementValidator validator = validating(undoing);
  validator.check(kPeer, kHero, along_x(0.0f));
  validator.advance(kTick);
  const MovementVerdict verdict = validator.check(kPeer, kHero, along_x(kTeleport * kFarAway));
  CHECK_FALSE(verdict.valid);
  CHECK(verdict.position.x == doctest::Approx(0.0f));
}

TEST_CASE("a teleport is never believed, not even in part") {
  MovementValidator validator = validating(rules(0.0f, kTeleport));
  validator.check(kPeer, kHero, along_x(0.0f));
  validator.advance(kCorrectionSeconds);
  const MovementVerdict verdict = validator.check(kPeer, kHero, along_x(kTeleport * kFarAway));
  CHECK_FALSE(verdict.valid);
  CHECK(verdict.position.x == doctest::Approx(0.0f));
  CHECK(validator.violations(kPeer) == 1);
  // Within the distance and its tolerance, a move is believed however far.
  CHECK(validator.check(kPeer, kHero, along_x(kTeleport * (1.0f + kTolerance * 0.5f))).valid);
}

TEST_CASE("a machine is let go after the violations the game allows, counted once per correction") {
  MovementValidator validator = validating(rules(0.0f, kTeleport, kKickAfter));
  validator.check(kPeer, kHero, along_x(0.0f));
  // Poses on their way before the correction reached it count once.
  CHECK_FALSE(validator.check(kPeer, kHero, along_x(kTeleport * kFarAway)).kick);
  CHECK_FALSE(validator.check(kPeer, kHero, along_x(kTeleport * kFarAway)).kick);
  CHECK(validator.violations(kPeer) == 1);
  validator.advance(kCorrectionSeconds);
  CHECK_FALSE(validator.check(kPeer, kHero, along_x(kTeleport * kFarAway)).kick);
  validator.advance(kCorrectionSeconds);
  CHECK(validator.check(kPeer, kHero, along_x(kTeleport * kFarAway)).kick);
  CHECK(validator.violations(kPeer) == kKickAfter);
  CHECK(validator.violations(kOtherPeer) == 0);
}

TEST_CASE("what a machine did is forgotten when it leaves, and its objects start over") {
  MovementValidator validator = validating(rules(0.0f, kTeleport));
  validator.check(kPeer, kHero, along_x(0.0f));
  validator.check(kPeer, kHero, along_x(kTeleport * kFarAway));
  REQUIRE(validator.violations(kPeer) == 1);
  validator.forget(kPeer);
  CHECK(validator.violations(kPeer) == 0);
  // Heard of afresh, an object is where its machine says.
  CHECK(validator.check(kPeer, kHero, along_x(kTeleport * kFarAway)).valid);
}

TEST_CASE("a stall of the host is not held against the moves that arrive at once after it") {
  MovementValidator validator = validating(rules(kMaxSpeed, 0.0f));
  validator.check(kPeer, kHero, along_x(0.0f));
  // The host stood still for two seconds (its clock moved on one frame): the others did not.
  validator.forgive(kStallSeconds, kStallSeconds);
  validator.advance(kTick);
  CHECK(validator.check(kPeer, kHero, along_x(kMaxSpeed * kStallSeconds)).valid);
}
