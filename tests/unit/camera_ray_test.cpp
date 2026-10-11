#include <doctest/doctest.h>

#include <cmath>

#include "scene/scene_config.hpp"
#include "scene/camera_ray.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr int kScreenWidth = 1600;
constexpr int kScreenHeight = 800;  // twice as wide as it is tall
constexpr float kAspect = 2.0f;
constexpr float kRightAngle = kPi * kHalf;  // a field of view whose half has a tangent of 1
constexpr float kQuarterTurnDegrees = 90.0f;

ScreenPoint point(float x, float y) { return {x, y, kScreenWidth, kScreenHeight}; }
ScreenPoint center() { return point(kScreenWidth * kHalf, kScreenHeight * kHalf); }

}  // namespace

TEST_CASE("the ray through the middle of the screen goes where the camera looks") {
  const Transform camera;
  CHECK(approx_equal(screen_ray_direction(camera, kRightAngle, center()), Vec3::forward(), kTolerance));

  Transform turned;
  turned.rotation = quat_from_euler_degrees({0.0f, kQuarterTurnDegrees, 0.0f});
  CHECK(approx_equal(screen_ray_direction(turned, kRightAngle, center()), turned.forward(), kTolerance));
}

TEST_CASE("the rays through the screen's edges open by the field of view, wider across than up") {
  const Transform camera;
  const Vec3 top = screen_ray_direction(camera, kRightAngle, point(kScreenWidth * kHalf, 0.0f));
  CHECK(approx_equal(top, normalize(Vec3::forward() + Vec3::up()), kTolerance));
  const Vec3 bottom = screen_ray_direction(camera, kRightAngle, point(kScreenWidth * kHalf, kScreenHeight));
  CHECK(approx_equal(bottom, normalize(Vec3::forward() - Vec3::up()), kTolerance));
  const Vec3 right = screen_ray_direction(camera, kRightAngle, point(kScreenWidth, kScreenHeight * kHalf));
  CHECK(approx_equal(right, normalize(Vec3::forward() + Vec3::right() * kAspect), kTolerance));
  CHECK(length(right) == doctest::Approx(1.0f));
}

TEST_CASE("a camera that gives no field of view uses the engine's") {
  const Transform camera;
  const ScreenPoint top = point(kScreenWidth * kHalf, 0.0f);
  const Vec3 expected = screen_ray_direction(camera, degrees_to_radians(config::kDefaultFovYDegrees), top);
  CHECK(approx_equal(screen_ray_direction(camera, 0.0f, top), expected, kTolerance));
}
