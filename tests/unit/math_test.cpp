#include <doctest/doctest.h>

#include "core/math.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

TEST_CASE("vector arithmetic") {
  const Vec3 a{1, 2, 3};
  const Vec3 b{4, 5, 6};
  CHECK(approx_equal(a + b, Vec3{5, 7, 9}, kTolerance));
  CHECK(approx_equal(b - a, Vec3{3, 3, 3}, kTolerance));
  CHECK(approx_equal(a * 2.0f, Vec3{2, 4, 6}, kTolerance));
  CHECK(dot(a, b) == doctest::Approx(32.0f));
  CHECK(approx_equal(cross(Vec3::right(), Vec3::up()), Vec3{0, 0, 1}, kTolerance));
  CHECK(length(normalize(Vec3{3, 0, 4})) == doctest::Approx(1.0f));
  CHECK(approx_equal(normalize(Vec3::zero()), Vec3::zero(), kTolerance));
  CHECK(approx_equal(safe_divide(Vec3{1, 1, 1}, Vec3{2, 0, 4}), Vec3{0.5f, 0, 0.25f}, kTolerance));
}

TEST_CASE("quaternion rotates vectors around an axis") {
  const Quat quarter_turn_y = quat_from_axis_angle(Vec3::up(), degrees_to_radians(90.0f));
  CHECK(approx_equal(rotate(quarter_turn_y, Vec3::forward()), Vec3{-1, 0, 0}, kTolerance));
  CHECK(approx_equal(rotate(Quat::identity(), Vec3{1, 2, 3}), Vec3{1, 2, 3}, kTolerance));
}

TEST_CASE("euler degrees round trip") {
  const Vec3 angles{30.0f, -45.0f, 60.0f};
  const Vec3 back = quat_to_euler_degrees(quat_from_euler_degrees(angles));
  CHECK(approx_equal(back, angles, 1e-2f));
}

TEST_CASE("look rotation points -Z at the target") {
  const Vec3 direction = normalize(Vec3{1, -1, -2});
  const Quat q = quat_look_rotation(direction, Vec3::up());
  CHECK(approx_equal(rotate(q, Vec3::forward()), direction, 1e-3f));
  // Looking straight down must not produce NaN.
  const Quat down = quat_look_rotation(Vec3{0, -1, 0}, Vec3::up());
  CHECK(approx_equal(rotate(down, Vec3::forward()), Vec3{0, -1, 0}, 1e-3f));
}

TEST_CASE("quaternion equality ignores sign") {
  const Quat q = quat_from_euler_degrees({10, 20, 30});
  CHECK(approx_equal(q, Quat{-q.x, -q.y, -q.z, -q.w}, kTolerance));
}

TEST_CASE("basis to quaternion covers every Shepperd branch") {
  // Rotations of 180 degrees make the trace negative and pick each diagonal branch in turn.
  for (const Vec3 axis : {Vec3::right(), Vec3::up(), Vec3{0, 0, 1}, normalize(Vec3{1, 1, 1})}) {
    for (const float degrees : {0.0f, 90.0f, 180.0f, -135.0f}) {
      CAPTURE(degrees);
      const Quat expected = quat_from_axis_angle(axis, degrees_to_radians(degrees));
      const Quat rebuilt =
          quat_from_basis(rotate(expected, Vec3::right()), rotate(expected, Vec3::up()), rotate(expected, {0, 0, 1}));
      CHECK(approx_equal(rebuilt, expected, 1e-4f));
    }
  }
}

TEST_CASE("euler conversion survives gimbal lock as a rotation") {
  constexpr float kRoll = 20.0f;
  constexpr float kYaw = 35.0f;
  constexpr float kRotationTolerance = 1e-3f;
  for (const float pitch : {90.0f, -90.0f, 89.995f, -89.995f, 89.5f}) {
    for (const float yaw : {0.0f, kYaw}) {
      CAPTURE(pitch);
      CAPTURE(yaw);
      const Quat q = quat_from_euler_degrees({kRoll, pitch, yaw});
      CHECK(approx_equal(quat_from_euler_degrees(quat_to_euler_degrees(q)), q, kRotationTolerance));
    }
  }
}
