#include <doctest/doctest.h>

#include <vector>

#include "editor/group_transform.hpp"

using namespace redgem;
using namespace redgem::group_transform;

namespace {

constexpr float kTolerance = 1e-4f;
constexpr float kQuarterTurn = kPi / 2.0f;
constexpr float kDouble = 2.0f;

Transform at(Vec3 position) {
  Transform placed;
  placed.position = position;
  return placed;
}

}  // namespace

TEST_CASE("the pivot of a group is at its middle or on its first object, turned like the first") {
  Transform turned = at({4, 0, 0});
  turned.rotation = quat_from_axis_angle(Vec3::up(), kQuarterTurn);
  turned.scale = {3, 3, 3};
  const std::vector<Transform> worlds = {turned, at({0, 2, 0}), at({2, 4, 6})};

  const Transform middle = pivot_of(worlds, PivotMode::Center);
  CHECK(approx_equal(middle.position, Vec3{2, 2, 2}, kTolerance));
  CHECK(approx_equal(middle.rotation, turned.rotation, kTolerance));
  CHECK(approx_equal(middle.scale, Vec3::one(), kTolerance));  // what the gizmo scales is the group's factor

  const Transform first = pivot_of(worlds, PivotMode::First);
  CHECK(approx_equal(first.position, Vec3{4, 0, 0}, kTolerance));

  const Transform none = pivot_of({}, PivotMode::Center);
  CHECK(approx_equal(none.position, Vec3::zero(), kTolerance));
}

TEST_CASE("moving the pivot moves every object by as much") {
  const std::vector<int> objects = {3, 5};
  Drag drag = begin_drag(objects, {at({1, 0, 0}), at({-1, 0, 2})}, PivotMode::Center);
  CHECK(approx_equal(drag.pivot_start.position, Vec3{0, 0, 1}, kTolerance));
  drag.pivot_now.position = drag.pivot_now.position + Vec3{0, 5, 0};
  const std::vector<Transform> moved = placements(drag);
  REQUIRE(moved.size() == 2);
  CHECK(approx_equal(moved[0].position, Vec3{1, 5, 0}, kTolerance));
  CHECK(approx_equal(moved[1].position, Vec3{-1, 5, 2}, kTolerance));
}

TEST_CASE("turning the pivot turns the objects around it, and each one with it") {
  Drag drag = begin_drag({0, 1}, {at({2, 0, 0}), at({-2, 0, 0})}, PivotMode::Center);
  const Quat quarter = quat_from_axis_angle(Vec3::up(), kQuarterTurn);
  drag.pivot_now.rotation = quarter * drag.pivot_start.rotation;
  const std::vector<Transform> turned = placements(drag);
  // A quarter turn about +Y takes +X to -Z.
  CHECK(approx_equal(turned[0].position, Vec3{0, 0, -2}, kTolerance));
  CHECK(approx_equal(turned[1].position, Vec3{0, 0, 2}, kTolerance));
  CHECK(approx_equal(turned[0].rotation, quarter, kTolerance));
}

TEST_CASE("turning around the first object leaves it where it is") {
  Drag drag = begin_drag({0, 1}, {at({2, 0, 0}), at({4, 0, 0})}, PivotMode::First);
  drag.pivot_now.rotation = quat_from_axis_angle(Vec3::up(), kQuarterTurn);
  const std::vector<Transform> turned = placements(drag);
  CHECK(approx_equal(turned[0].position, Vec3{2, 0, 0}, kTolerance));
  CHECK(approx_equal(turned[1].position, Vec3{2, 0, -2}, kTolerance));
}

TEST_CASE("scaling the pivot spreads the objects from it and scales each one") {
  Transform small = at({1, 0, 0});
  small.scale = {0.5f, 0.5f, 0.5f};
  Drag drag = begin_drag({0, 1}, {small, at({3, 0, 0})}, PivotMode::Center);
  drag.pivot_now.scale = {kDouble, kDouble, kDouble};
  const std::vector<Transform> scaled = placements(drag);
  CHECK(approx_equal(scaled[0].position, Vec3{0, 0, 0}, kTolerance));
  CHECK(approx_equal(scaled[1].position, Vec3{4, 0, 0}, kTolerance));
  CHECK(approx_equal(scaled[0].scale, Vec3{1, 1, 1}, kTolerance));
  CHECK(approx_equal(scaled[1].scale, Vec3{2, 2, 2}, kTolerance));
}

TEST_CASE("one object dragged alone goes exactly where its pivot goes") {
  Transform crate = at({1, 2, 3});
  crate.rotation = quat_from_axis_angle(Vec3::right(), kQuarterTurn);
  crate.scale = {2, 1, 1};
  Drag drag = begin_drag({0}, {crate}, PivotMode::Center);
  drag.pivot_now.position = {5, 5, 5};
  const Transform moved = placements(drag).front();
  CHECK(approx_equal(moved.position, Vec3{5, 5, 5}, kTolerance));
  CHECK(approx_equal(moved.rotation, crate.rotation, kTolerance));
  CHECK(approx_equal(moved.scale, crate.scale, kTolerance));
}

TEST_CASE("an uneven scale of the group stretches a turned object along the pivot's axes") {
  Transform turned = at({1, 0, 0});
  turned.rotation = quat_from_axis_angle(Vec3::up(), kQuarterTurn);             // its own z lies along the world's x
  Drag drag = begin_drag({0, 1}, {at({-1, 0, 0}), turned}, PivotMode::Center);  // the pivot's axes are the world's
  drag.pivot_now.scale = {kDouble, 1.0f, 1.0f};
  const std::vector<Transform> stretched = placements(drag);
  CHECK(approx_equal(stretched[0].position, Vec3{-2, 0, 0}, kTolerance));
  CHECK(approx_equal(stretched[0].scale, Vec3{2, 1, 1}, kTolerance));
  CHECK(approx_equal(stretched[1].position, Vec3{2, 0, 0}, kTolerance));
  CHECK(approx_equal(stretched[1].scale, Vec3{1, 1, 2}, kTolerance));  // longer across the world's x
  CHECK(approx_equal(stretched[1].rotation, turned.rotation, kTolerance));
}

TEST_CASE("an even scale of the group keeps a mirrored object mirrored") {
  Transform mirrored = at({1, 0, 0});
  mirrored.scale = {-1, 1, 1};
  Drag drag = begin_drag({0}, {mirrored}, PivotMode::First);
  drag.pivot_now.scale = {kDouble, kDouble, kDouble};
  CHECK(approx_equal(placements(drag).front().scale, Vec3{-2, 2, 2}, kTolerance));
}
