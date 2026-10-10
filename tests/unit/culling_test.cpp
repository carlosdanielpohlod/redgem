#include <doctest/doctest.h>

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
