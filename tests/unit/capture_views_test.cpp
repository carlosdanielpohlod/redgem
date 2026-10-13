#include <doctest/doctest.h>

#include "render/capture_views.hpp"
#include "render/render_math.hpp"
#include "test_support.hpp"

#include <raymath.h>  // after raylib.h (through capture_views.hpp): it relies on raylib's own types

using namespace redgem;
using namespace redgem::render_math;
using redgem::test::kTolerance;

namespace {

constexpr Vec3 kPlace{10.0f, 2.0f, -5.0f};  // somewhere in the world, meters
constexpr float kDouble = 2.0f;
constexpr float kShadowsAsked = 0.8f;
constexpr Vec3 kHighSun{0.0f, 1.0f, 0.0f};
constexpr Vec3 kSlantedSun{0.6f, 0.8f, 0.0f};
constexpr Vec3 kSetSun{1.0f, 0.0f, 0.0f};
constexpr float kStep = 0.5f;
constexpr float kAboveStep = 7.3f;
constexpr float kSnapped = 7.0f;
constexpr float kBelowZero = -0.2f;
constexpr float kSnappedBelowZero = -0.5f;
constexpr Vector3 kOrigin{0.0f, 0.0f, 0.0f};
constexpr Vector3 kLookingAhead{0.0f, 0.0f, -1.0f};
constexpr float kTexel = kShadowExtent / static_cast<float>(kShadowMapSize);  // meters of ground per shadow texel
constexpr float kAheadOfCamera = kShadowExtent * kShadowFocusAhead;           // meters
constexpr float kQuarterTexel = kTexel * 0.25f;
constexpr float kHalfTexel = kTexel * 0.5f;
constexpr float kTexelAndAHalf = kTexel * 1.5f;
constexpr float kLowCamera = 0.3f;  // meters above the floor
constexpr float kHighCamera = 8.0f;
constexpr int kLastFace = kCubeFaces - 1;

Vec3 across_the_map(Vector3 v) { return {v.x, 0.0f, v.z}; }

}  // namespace

TEST_CASE("the sun casts shadows as dark as asked, and none once it has set") {
  CHECK(sun_shadow_strength(kHighSun, kShadowsAsked) == kShadowsAsked);
  CHECK(sun_shadow_strength(kSetSun, kShadowsAsked) == kNoShadows);
  CHECK(sun_shadow_strength(-kHighSun, kShadowsAsked) == kNoShadows);
}

TEST_CASE("values snap down to whole steps, below zero too") {
  CHECK(snapped_down(kAboveStep, kStep) == doctest::Approx(kSnapped));
  CHECK(snapped_down(kSnapped, kStep) == doctest::Approx(kSnapped));
  CHECK(snapped_down(kBelowZero, kStep) == doctest::Approx(kSnappedBelowZero));
}

TEST_CASE("a sun straight overhead orients its shadow map by another axis") {
  const Vector3 overhead = shadow_map_vertical(to_raylib(kHighSun));
  CHECK(approx_equal(to_vec3(overhead), to_vec3(kForwardAxis), kTolerance));
  const Vector3 slanted = shadow_map_vertical(to_raylib(kSlantedSun));
  CHECK(approx_equal(to_vec3(slanted), to_vec3(kUpAxis), kTolerance));
}

TEST_CASE("the sun looks at the ground ahead of the camera, from far back along its light") {
  const SunView view = fit_sun_view(kOrigin, kLookingAhead, kSlantedSun);
  const Vec3 ahead{0.0f, 0.0f, -kAheadOfCamera};
  // Within a texel (the snapping) of the point ahead, on each of the map's two axes.
  CHECK(length(to_vec3(view.target) - ahead) <= kTexel * kDouble);
  CHECK(approx_equal(to_vec3(view.position) - to_vec3(view.target), kSlantedSun * kShadowSunDistance, kTolerance));
  CHECK(approx_equal(to_vec3(view.up), to_vec3(kUpAxis), kTolerance));
}

TEST_CASE("the shadow map moves in whole texels, so its edges do not shimmer") {
  const auto target_from = [](float camera_x) {
    const Vector3 position{camera_x, 0.0f, 0.0f};
    return across_the_map(fit_sun_view(position, Vector3Add(position, kLookingAhead), kHighSun).target);
  };
  // Two camera places within the same texel: the map stays where it is.
  CHECK(approx_equal(target_from(kQuarterTexel), target_from(kHalfTexel), kEpsilon));
  // One texel further: the map follows by exactly one texel.
  const Vec3 moved = target_from(kTexelAndAHalf) - target_from(kHalfTexel);
  CHECK(length(moved) == doctest::Approx(kTexel));
}

TEST_CASE("each cube face's picture is square to the direction it looks in") {
  for (const CubeFace& face : kCubeFaceViews) {
    const FaceAxes axes = face_axes(face);
    CHECK(Vector3DotProduct(axes.right, face.forward) == doctest::Approx(0.0f));
    CHECK(Vector3DotProduct(axes.up, face.forward) == doctest::Approx(0.0f));
    CHECK(Vector3DotProduct(axes.right, axes.up) == doctest::Approx(0.0f));
    CHECK(Vector3Length(axes.right) == doctest::Approx(1.0f));
    CHECK(approx_equal(to_vec3(axes.up), to_vec3(face.up), kTolerance));
  }
}

TEST_CASE("reflections are captured from the camera, but not from below eye height") {
  CHECK(reflection_eye({kPlace.x, kLowCamera, kPlace.z}).y == kReflectionEyeHeight);
  CHECK(reflection_eye({kPlace.x, kHighCamera, kPlace.z}).y == kHighCamera);
  CHECK(reflection_eye({kPlace.x, kLowCamera, kPlace.z}).x == kPlace.x);
  CHECK(reflection_eye({kPlace.x, kLowCamera, kPlace.z}).z == kPlace.z);
}

TEST_CASE("the cube's faces are captured in turn, and reflections wait for all six") {
  CHECK(face_after(0) == 1);
  CHECK(face_after(kLastFace) == 0);
  CHECK(reflection_strength(0) == kReflectionsOff);
  CHECK(reflection_strength(kLastFace) == kReflectionsOff);
  CHECK(reflection_strength(kCubeFaces) == kReflectionsOn);
}
