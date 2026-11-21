// The upright capsules a character gets when it changes size: how tall and wide they are, and
// where their bottom sits.

#include <doctest/doctest.h>

#include <string>
#include <variant>

#include "physics/jolt/jolt_runtime.hpp"
#include "physics/jolt/jolt_shapes.hpp"

using namespace redgem;
using namespace redgem::jolt;

namespace {

constexpr float kTolerance = 0.001f;
constexpr Vec3 kFeet{0.1f, -0.9f, 0.2f};
constexpr CapsuleSize kCrouching{1.0f, 0.4f};
// Only as tall as it is wide: no room for the shortest cylinder a capsule keeps.
constexpr CapsuleSize kBall{0.8f, 0.4f};

ShapeInfo made(Vec3 bottom, CapsuleSize size) {
  auto info = capsule_standing_on(bottom, size);
  if (auto* error = std::get_if<std::string>(&info)) FAIL(*error);
  return std::get<ShapeInfo>(info);
}

void check_near(Vec3 a, Vec3 b) {
  CHECK(a.x == doctest::Approx(b.x).epsilon(kTolerance));
  CHECK(a.y == doctest::Approx(b.y).epsilon(kTolerance));
  CHECK(a.z == doctest::Approx(b.z).epsilon(kTolerance));
}

}  // namespace

TEST_CASE("a capsule made to stand on a point keeps its bottom there, at the size asked") {
  const Runtime runtime;
  const ShapeInfo info = made(kFeet, kCrouching);
  check_near(collider_bottom(info), kFeet);
  CHECK(capsule_size_of(info).height == doctest::Approx(kCrouching.height));
  CHECK(capsule_size_of(info).radius == doctest::Approx(kCrouching.radius));
  CHECK(info.center_offset.y == doctest::Approx(kFeet.y + kCrouching.height / 2));
}

TEST_CASE("a capsule too short for its width grows upwards, never below its bottom") {
  const Runtime runtime;
  const ShapeInfo info = made(kFeet, kBall);
  check_near(collider_bottom(info), kFeet);
  CHECK(capsule_size_of(info).height > kBall.height);
}

TEST_CASE("a box collider is seen as a capsule of its bounds") {
  ShapeInfo box;
  box.half_extents = {0.3f, 1.0f, 0.5f};
  box.center_offset = {0.0f, 1.0f, 0.0f};
  CHECK(capsule_size_of(box).height == doctest::Approx(2.0f));
  CHECK(capsule_size_of(box).radius == doctest::Approx(0.5f));
  check_near(collider_bottom(box), Vec3{0.0f, 0.0f, 0.0f});
}
