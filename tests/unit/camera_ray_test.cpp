#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "core/letterbox.hpp"
#include "scene/camera_ray.hpp"
#include "scene/scene_config.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr int kScreenWidth = 1600;
constexpr int kScreenHeight = 800;  // twice as wide as it is tall
constexpr float kAspect = 2.0f;
constexpr float kRightAngle = kPi * kHalf;  // a field of view whose half has a tangent of 1
constexpr float kQuarterTurnDegrees = 90.0f;
constexpr float kPixelTolerance = 0.01f;
constexpr float kOrthoHeight = 10.0f;  // meters the orthographic picture spans from top to bottom
constexpr float kFar = 25.0f;          // meters down a ray where its point is taken back to the screen

const Lens kPerspective{kRightAngle, 0.0f};
const Lens kOrthographic{0.0f, kOrthoHeight};

ScreenFrame screen() { return {kScreenWidth, kScreenHeight, 0, 0}; }
ScreenPoint center() { return {kScreenWidth * kHalf, kScreenHeight * kHalf}; }
Vec3 direction(const Transform& camera, float fov, ScreenPoint point) {
  return screen_ray(camera, Lens{fov, 0.0f}, screen(), point).direction;
}

// A camera somewhere, turned every way: no axis of the world lines up with it by chance.
Transform turned_camera() {
  Transform camera;
  camera.position = {3.0f, 2.0f, -7.0f};
  camera.rotation = quat_from_euler_degrees({-20.0f, 35.0f, 10.0f});
  return camera;
}

bool near(ScreenPoint a, ScreenPoint b) {
  return std::abs(a.x - b.x) < kPixelTolerance && std::abs(a.y - b.y) < kPixelTolerance;
}

// Screens of several shapes, the world's picture drawn at several shares of them.
std::vector<ScreenFrame> frames() {
  return {{1600, 800, 0, 0},   {800, 600, 400, 300},  {1080, 1920, 540, 960},
          {333, 177, 125, 66}, {1280, 720, 960, 540}, {800, 400, 400, 400}};
}

std::vector<ScreenPoint> points_of(const ScreenFrame& frame) {
  const float w = static_cast<float>(frame.screen_width);
  const float h = static_cast<float>(frame.screen_height);
  return {{w * kHalf, h * kHalf}, {0.0f, 0.0f}, {w, h}, {w * 0.1f, h * 0.8f}, {w * 0.9f, h * 0.3f}};
}

// Whether a point of every screen comes back from the world, where its ray goes, to itself.
void check_round_trips(const Lens& lens) {
  const Transform camera = turned_camera();
  for (const ScreenFrame& frame : frames()) {
    for (const ScreenPoint& point : points_of(frame)) {
      const CameraRay ray = screen_ray(camera, lens, frame, point);
      const auto back = world_to_screen(camera, lens, frame, ray.origin + ray.direction * kFar);
      REQUIRE(back.has_value());
      CHECK_MESSAGE(near(*back, point), frame.screen_width << "x" << frame.screen_height << " at " << point.x << ","
                                                           << point.y << " came back at " << back->x << "," << back->y);
    }
  }
}

}  // namespace

TEST_CASE("the ray through the middle of the screen goes where the camera looks") {
  const Transform camera;
  CHECK(approx_equal(direction(camera, kRightAngle, center()), Vec3::forward(), kTolerance));

  Transform turned;
  turned.rotation = quat_from_euler_degrees({0.0f, kQuarterTurnDegrees, 0.0f});
  CHECK(approx_equal(direction(turned, kRightAngle, center()), turned.forward(), kTolerance));
}

TEST_CASE("the rays through the screen's edges open by the field of view, wider across than up") {
  const Transform camera;
  const Vec3 top = direction(camera, kRightAngle, {kScreenWidth * kHalf, 0.0f});
  CHECK(approx_equal(top, normalize(Vec3::forward() + Vec3::up()), kTolerance));
  const Vec3 bottom = direction(camera, kRightAngle, {kScreenWidth * kHalf, kScreenHeight});
  CHECK(approx_equal(bottom, normalize(Vec3::forward() - Vec3::up()), kTolerance));
  const Vec3 right = direction(camera, kRightAngle, {kScreenWidth, kScreenHeight * kHalf});
  CHECK(approx_equal(right, normalize(Vec3::forward() + Vec3::right() * kAspect), kTolerance));
  CHECK(length(right) == doctest::Approx(1.0f));
}

TEST_CASE("a camera that gives no field of view uses the engine's") {
  const Transform camera;
  const ScreenPoint top{kScreenWidth * kHalf, 0.0f};
  const Vec3 expected = direction(camera, degrees_to_radians(config::kDefaultFovYDegrees), top);
  CHECK(approx_equal(direction(camera, 0.0f, top), expected, kTolerance));
}

TEST_CASE("a point of the world is seen where the ray through that pixel goes, on screens of every shape") {
  check_round_trips(kPerspective);
  check_round_trips(Lens{});  // the engine's field of view
  check_round_trips(kOrthographic);
}

TEST_CASE("what the camera looks straight at is in the middle of the screen, the top of its view at the top") {
  const Transform camera;
  const auto ahead = world_to_screen(camera, kPerspective, screen(), Vec3::forward() * kFar);
  REQUIRE(ahead.has_value());
  CHECK(near(*ahead, center()));
  // (A right angle of view: one meter up, one meter away, is the top of the picture.)
  const auto top = world_to_screen(camera, kPerspective, screen(), Vec3::forward() + Vec3::up());
  REQUIRE(top.has_value());
  CHECK(near(*top, {kScreenWidth * kHalf, 0.0f}));
}

TEST_CASE("points behind the camera or nearer than its near plane are not on the screen") {
  const Transform camera = turned_camera();
  const Vec3 ahead = camera.forward();
  CHECK_FALSE(world_to_screen(camera, kPerspective, screen(), camera.position - ahead).has_value());
  CHECK_FALSE(world_to_screen(camera, kPerspective, screen(), camera.position).has_value());
  CHECK_FALSE(
      world_to_screen(camera, kPerspective, screen(), camera.position + ahead * (kNearPlane * kHalf)).has_value());
  CHECK(world_to_screen(camera, kPerspective, screen(), camera.position + ahead * (kNearPlane * 2.0f)).has_value());
  CHECK_FALSE(world_to_screen(camera, kOrthographic, screen(), camera.position - ahead).has_value());
}

TEST_CASE("a point in front of the camera but out of its view is placed off the screen") {
  const Transform camera;
  const auto left = world_to_screen(camera, kPerspective, screen(), Vec3::forward() - Vec3::right() * kFar);
  REQUIRE(left.has_value());
  CHECK(left->x < 0.0f);
}

TEST_CASE("an orthographic picture shows things the same size however far away they are") {
  const Transform camera;
  const Vec3 up = Vec3::up() * (kOrthoHeight * kHalf);  // the top of the picture
  const auto near_top = world_to_screen(camera, kOrthographic, screen(), Vec3::forward() + up);
  const auto far_top = world_to_screen(camera, kOrthographic, screen(), Vec3::forward() * kFar + up);
  REQUIRE(near_top.has_value());
  REQUIRE(far_top.has_value());
  CHECK(near(*near_top, {kScreenWidth * kHalf, 0.0f}));
  CHECK(near(*far_top, *near_top));
}

TEST_CASE("a picture drawn at another shape than the screen's sits in its middle, between bars") {
  const Transform camera;
  const ScreenFrame square_picture{800, 400, 400, 400};  // shown 400 wide, from x = 200 to 600
  const auto right_edge = world_to_screen(camera, kPerspective, square_picture, Vec3::forward() + Vec3::right());
  REQUIRE(right_edge.has_value());
  CHECK(near(*right_edge, {600.0f, 200.0f}));
}

TEST_CASE("a screen of no size and points too far off to place are not on the screen") {
  const Transform camera;
  CHECK_FALSE(world_to_screen(camera, kPerspective, ScreenFrame{}, Vec3::forward()).has_value());
  const float huge = std::numeric_limits<float>::max();
  CHECK_FALSE(world_to_screen(camera, kPerspective, screen(), {huge, 0.0f, -kNearPlane * 2.0f}).has_value());
  const float nan = std::numeric_limits<float>::quiet_NaN();
  CHECK_FALSE(world_to_screen(camera, kPerspective, screen(), {0.0f, 0.0f, nan}).has_value());
  CHECK_FALSE(world_to_screen(camera, kPerspective, screen(), {nan, 0.0f, -1.0f}).has_value());
}

TEST_CASE("a shake moves where the camera sees from along the camera's own axes, not the world's") {
  const Transform camera = turned_camera();
  Transform shake;
  shake.position = {1.0f, 0.0f, 0.0f};  // a meter to its right
  shake.rotation = quat_from_euler_degrees({0.0f, kQuarterTurnDegrees, 0.0f});
  const Transform seen = viewed_from(camera, shake);
  CHECK(approx_equal(seen.position, camera.position + camera.right(), kTolerance));
  CHECK(approx_equal(seen.forward(), -camera.right(), kTolerance));  // a quarter turn to its left
  CHECK(approx_equal(viewed_from(camera, Transform{}).forward(), camera.forward(), kTolerance));
}

TEST_CASE("a picture is shown as large as fits, in the middle of the window") {
  const Letterbox wide = fit_picture(800.0f, 400.0f, 400.0f, 400.0f);
  CHECK(wide.x == doctest::Approx(200.0f));
  CHECK(wide.y == doctest::Approx(0.0f));
  CHECK(wide.width == doctest::Approx(400.0f));
  const Letterbox tall = fit_picture(400.0f, 800.0f, 200.0f, 100.0f);
  CHECK(tall.y == doctest::Approx(300.0f));
  CHECK(tall.height == doctest::Approx(200.0f));
}
