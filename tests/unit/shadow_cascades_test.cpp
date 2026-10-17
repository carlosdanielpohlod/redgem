// Cascaded sun shadows: where the slices of the view end, the spheres around them, and that
// their tiles move in whole texels only.
#include <doctest/doctest.h>

#include <cmath>

#include "render/render_math.hpp"
#include "render/shadow_cascades.hpp"
#include "test_support.hpp"

#include <raymath.h>  // after raylib.h (through shadow_cascades.hpp): it relies on raylib's own types

using namespace redgem;
using namespace redgem::render_math;

namespace {

constexpr float kNear = 0.5f;
constexpr float kDistance = 200.0f;
constexpr int kFour = 4;
constexpr float kTanHalfFov = 0.6f;
constexpr float kAspect = 16.0f / 9.0f;
constexpr Vec3 kSlantedSun{0.6f, 0.8f, 0.0f};
constexpr float kSliceFrom = 10.0f;
constexpr float kSliceTo = 40.0f;
constexpr float kNudge = 0.001f;  // meters: far less than a texel

CascadeCamera camera_at(Vector3 position, Vector3 forward) {
  return {position, Vector3Add(position, forward), Vector3Normalize(forward), kTanHalfFov, kAspect, kNear};
}

// The far corners of a slice ending `to` meters along a camera looking down -Z from the origin.
float farthest_corner_from(const CascadeCamera& camera, float to, Vec3 center) {
  const float up = to * camera.tan_half_fov;
  const float side = up * camera.aspect;
  return length(Vec3{side, up, -to} - center);
}

}  // namespace

TEST_CASE("the slices of the view deepen away from the camera, the last ending where shadows end") {
  const CascadeEnds ends = cascade_ends(kFour, kNear, kDistance);
  CHECK(ends[3] == doctest::Approx(kDistance));
  for (int i = 1; i < kFour; ++i) CHECK(ends[i] > ends[i - 1]);
  CHECK(ends[1] - ends[0] < ends[3] - ends[2]);  // nearer ones thinner
  CHECK(cascade_ends(1, kNear, kDistance)[0] == doctest::Approx(kDistance));
}

TEST_CASE("the sphere around a slice holds its corners, and is the same however the camera turns") {
  const CascadeCamera ahead = camera_at({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
  const SliceSphere sphere = slice_sphere(ahead, kSliceFrom, kSliceTo);
  const Vec3 center{0.0f, 0.0f, -sphere.center};
  CHECK(farthest_corner_from(ahead, kSliceTo, center) <= sphere.radius + test::kTolerance);
  CHECK(farthest_corner_from(ahead, kSliceFrom, center) <= sphere.radius + test::kTolerance);
  const CascadeCamera turned = camera_at({0.0f, 0.0f, 0.0f}, {1.0f, -0.3f, 0.2f});
  CHECK(slice_sphere(turned, kSliceFrom, kSliceTo).radius == doctest::Approx(sphere.radius));
}

TEST_CASE("one cascade is the single shadow map; more share it, a quarter each") {
  const CascadeCamera camera = camera_at({0.0f, 2.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
  const Cascades single = fit_cascades(camera, kSlantedSun, 1, kDistance);
  REQUIRE(single.count == 1);
  CHECK(single.cascades[0].extent == doctest::Approx(kShadowExtent));
  CHECK(single.cascades[0].size == doctest::Approx(1.0f));
  CHECK(cascade_scale(single.cascades[0], 1) == doctest::Approx(1.0f));
  const Cascades four = fit_cascades(camera, kSlantedSun, kFour, kDistance);
  REQUIRE(four.count == kFour);
  CHECK(four.cascades[3].corner.x == doctest::Approx(kHalf));
  CHECK(four.cascades[3].corner.y == doctest::Approx(kHalf));
  CHECK(four.cascades[3].extent > four.cascades[0].extent);
  CHECK(cascade_texels(kFour) == kShadowMapSize / 2);
  CHECK(fit_cascades(camera, kSlantedSun, kFour + 3, kDistance).count == kFour);
}

TEST_CASE("a cascade moves in whole texels: a camera moving less than one leaves it where it was") {
  const auto target_of = [](float x) {
    return fit_cascades(camera_at({x, 2.0f, 0.0f}, {0.0f, 0.0f, -1.0f}), kSlantedSun, kFour, kDistance).cascades[0];
  };
  const Cascade still = target_of(0.0f);
  const float texel = still.extent / static_cast<float>(cascade_texels(kFour));
  const Cascade nudged = target_of(kNudge);
  CHECK(Vector3Distance(still.view.target, nudged.view.target) < texel * kHalf);
  // Wherever it went, it is a whole number of texels across the sun's view from where it was.
  const Cascade moved = target_of(texel * 3.3f);
  const Vector3 right = Vector3Normalize(Vector3CrossProduct(still.view.up, to_raylib(kSlantedSun)));
  const float across = Vector3DotProduct(Vector3Subtract(moved.view.target, still.view.target), right) / texel;
  CHECK(std::fabs(across - std::round(across)) < 0.01f);
}
