#include <doctest/doctest.h>

#include "scene/transform.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

TEST_CASE("child transform is combined with its parent") {
  Transform parent;
  parent.position = {10, 0, 0};
  parent.rotation = quat_from_axis_angle(Vec3::up(), degrees_to_radians(90.0f));
  parent.scale = {2, 2, 2};
  Transform child;
  child.position = {0, 0, -1};

  const Transform world = combine(parent, child);
  CHECK(approx_equal(world.position, Vec3{8, 0, 0}, kTolerance));
  CHECK(approx_equal(world.scale, Vec3{2, 2, 2}, kTolerance));
}

TEST_CASE("relative_to inverts combine") {
  Transform parent;
  parent.position = {1, 2, 3};
  parent.rotation = quat_from_euler_degrees({10, 20, 30});
  parent.scale = {1, 2, 3};
  Transform child;
  child.position = {4, 5, 6};
  child.rotation = quat_from_euler_degrees({-5, 15, 45});
  child.scale = {0.5f, 0.5f, 0.5f};

  const Transform local = relative_to(parent, combine(parent, child));
  CHECK(approx_equal(local.position, child.position, 1e-3f));
  CHECK(approx_equal(local.rotation, child.rotation, 1e-3f));
  CHECK(approx_equal(local.scale, child.scale, 1e-3f));
}

TEST_CASE("direction helpers follow the rotation") {
  Transform t;
  CHECK(approx_equal(t.forward(), Vec3::forward(), kTolerance));
  CHECK(approx_equal(t.right(), Vec3::right(), kTolerance));
  t.rotation = quat_from_axis_angle(Vec3::up(), degrees_to_radians(180.0f));
  CHECK(approx_equal(t.forward(), Vec3{0, 0, 1}, kTolerance));
}

TEST_CASE("relative_to with a rotated, non-uniformly scaled parent keeps positions exact") {
  Transform parent;
  parent.rotation = quat_from_euler_degrees({0, 45, 0});
  parent.scale = {1, 2, 4};
  Transform world;
  world.position = {3, -1, 2};
  CHECK(approx_equal(combine(parent, relative_to(parent, world)).position, world.position, 1e-4f));
}

TEST_CASE("TRS matrices decompose back into the transform") {
  constexpr Vec3 kPosition{1, 2, 3};
  constexpr Vec3 kEulerDegrees{10, 20, 30};
  constexpr Vec3 kScale{2, 3, 4};
  constexpr float kDecomposeTolerance = 1e-4f;
  constexpr float kZero = 0.0f;
  constexpr float kOne = 1.0f;
  Transform t;
  t.position = kPosition;
  t.rotation = quat_from_euler_degrees(kEulerDegrees);
  t.scale = kScale;
  // Build the column-major matrix by transforming the basis vectors.
  const Vec3 x = rotate(t.rotation, Vec3::right()) * t.scale.x;
  const Vec3 y = rotate(t.rotation, Vec3::up()) * t.scale.y;
  const Vec3 z = rotate(t.rotation, Vec3::back()) * t.scale.z;
  const float m[] = {x.x, x.y, x.z, kZero, y.x,         y.y,         y.z,         kZero,
                     z.x, z.y, z.z, kZero, kPosition.x, kPosition.y, kPosition.z, kOne};
  const Transform back = transform_from_matrix(m);
  CHECK(approx_equal(back.position, t.position, kDecomposeTolerance));
  CHECK(approx_equal(back.scale, t.scale, kDecomposeTolerance));
  CHECK(approx_equal(back.rotation, t.rotation, kDecomposeTolerance));
}

TEST_CASE("a placement mirrors when an odd number of its scales are negative") {
  Transform t;
  CHECK_FALSE(mirrors(t));
  t.scale = {-1, 1, 1};
  CHECK(mirrors(t));
  t.scale = {-1, -1, 1};  // two mirrors make a half turn
  CHECK_FALSE(mirrors(t));
  t.scale = {-2, -0.5f, -3};
  CHECK(mirrors(t));

  Transform parent;
  parent.scale = {1, 1, -1};
  Transform child;
  child.scale = {2, 2, 2};
  CHECK(mirrors(combine(parent, child)));
}

TEST_CASE("a mirroring matrix is split into a rotation and a negative x scale") {
  constexpr float kHalf = 0.5f;
  constexpr float kSplitTolerance = 1e-4f;
  constexpr float kNoTranslation = 0.0f;
  constexpr float kHomogeneous = 1.0f;
  // Scale (1, 1, -2) after a quarter turn about y: the z axis points the other way.
  const Transform placed{Vec3{1, 2, 3}, quat_from_axis_angle(Vec3::up(), degrees_to_radians(90.0f)), Vec3{1, 1, -2}};
  const Vec3 x = rotate(placed.rotation, Vec3{placed.scale.x, 0, 0});
  const Vec3 y = rotate(placed.rotation, Vec3{0, placed.scale.y, 0});
  const Vec3 z = rotate(placed.rotation, Vec3{0, 0, placed.scale.z});
  const Vec3 p = placed.position;
  const float m[] = {x.x, x.y, x.z, kNoTranslation, y.x, y.y, y.z, kNoTranslation,
                     z.x, z.y, z.z, kNoTranslation, p.x, p.y, p.z, kHomogeneous};
  const Transform split = transform_from_matrix(m);
  CHECK(mirrors(split));
  CHECK(split.scale.x < 0.0f);
  CHECK(approx_equal(split.position, placed.position, kSplitTolerance));
  // Whatever the split, it places every point where the matrix does.
  for (const Vec3 point : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}, Vec3{kHalf, -kHalf, 2}}) {
    const Vec3 expected = placed.position + rotate(placed.rotation, hadamard(placed.scale, point));
    const Vec3 got = split.position + rotate(split.rotation, hadamard(split.scale, point));
    CHECK(approx_equal(got, expected, kSplitTolerance));
  }
}
