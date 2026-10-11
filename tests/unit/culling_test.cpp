#include <doctest/doctest.h>

#include <limits>

#include "render/culling.hpp"

using namespace redgem;

namespace {

// The identity matrix views the cube from -1 to 1 on every axis.
constexpr float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

}  // namespace

TEST_CASE("spheres outside the view volume are culled, touching ones are kept") {
  const Frustum frustum = frustum_from_matrix(kIdentity);
  CHECK(sphere_in_frustum(frustum, Vec3::zero(), 0.5f));
  CHECK(sphere_in_frustum(frustum, {1.5f, 0.0f, 0.0f}, 1.0f));   // crosses the right side
  CHECK_FALSE(sphere_in_frustum(frustum, {3.0f, 0.0f, 0.0f}, 1.0f));
  CHECK_FALSE(sphere_in_frustum(frustum, {0.0f, -2.6f, 0.0f}, 1.5f));
  CHECK_FALSE(sphere_in_frustum(frustum, {0.0f, 0.0f, 5.0f}, 0.1f));
}

TEST_CASE("a translated view volume moves with its matrix") {
  // Column-major translation by -10 on x: the visible cube is now centered at x = 10.
  constexpr float kShifted[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -10, 0, 0, 1};
  const Frustum frustum = frustum_from_matrix(kShifted);
  CHECK(sphere_in_frustum(frustum, {10.0f, 0.0f, 0.0f}, 0.1f));
  CHECK_FALSE(sphere_in_frustum(frustum, Vec3::zero(), 0.1f));
}

TEST_CASE("small far things are not worth drawing; near or big ones are") {
  constexpr float kMinAngle = 0.01f;
  const Vec3 viewer = Vec3::zero();
  CHECK(too_small_to_see({0.0f, 0.0f, -100.0f}, 0.05f, viewer, kMinAngle));        // a bolt at 100 m
  CHECK_FALSE(too_small_to_see({0.0f, 0.0f, -100.0f}, 5.0f, viewer, kMinAngle));    // a house at 100 m
  CHECK_FALSE(too_small_to_see({0.0f, 0.0f, -2.0f}, 0.05f, viewer, kMinAngle));     // a bolt up close
  CHECK_FALSE(too_small_to_see({0.0f, 0.0f, -0.01f}, 0.05f, viewer, kMinAngle));    // the viewer is inside it
}

TEST_CASE("levels of detail are drawn only within their range of distances") {
  constexpr float kNear = 15.0f;
  constexpr float kFar = 45.0f;
  const Vec3 viewer{0.0f, 2.0f, 0.0f};
  const Vec3 close{0.0f, 2.0f, -5.0f};
  const Vec3 middle{0.0f, 2.0f, -30.0f};
  const Vec3 distant{0.0f, 2.0f, -80.0f};
  // The detailed version, the middle one and the light one of the same model.
  CHECK_FALSE(outside_distance_range(close, viewer, 0.0f, kNear));
  CHECK(outside_distance_range(middle, viewer, 0.0f, kNear));
  CHECK_FALSE(outside_distance_range(middle, viewer, kNear, kFar));
  CHECK(outside_distance_range(close, viewer, kNear, kFar));
  CHECK(outside_distance_range(distant, viewer, kNear, kFar));
  CHECK_FALSE(outside_distance_range(distant, viewer, kFar, std::numeric_limits<float>::infinity()));
  // Exactly at a boundary, one version shows: never both, never none.
  const Vec3 boundary{0.0f, 2.0f, -kNear};
  CHECK(outside_distance_range(boundary, viewer, 0.0f, kNear));
  CHECK_FALSE(outside_distance_range(boundary, viewer, kNear, kFar));
}
