#include <doctest/doctest.h>

#include "editor/box_select.hpp"

using namespace redgem;
using namespace redgem::box_select;

namespace {

constexpr float kTolerance = 1e-3f;
constexpr float kWidth = 800.0f;
constexpr float kHeight = 600.0f;
constexpr float kFovDegrees = 90.0f;  // the view's top edge is as far up as the point is ahead

// A camera at the origin looking down -Z, Y up.
ViewCamera looking_ahead() { return {Vec3::zero(), {0, 0, -1}, Vec3::up(), kFovDegrees, kWidth, kHeight}; }

}  // namespace

TEST_CASE("a point straight ahead shows in the middle of the view, others off to their side") {
  const ViewCamera camera = looking_ahead();
  const auto middle = to_view(camera, {0, 0, -10});
  REQUIRE(middle);
  CHECK(middle->x == doctest::Approx(kWidth / 2).epsilon(kTolerance));
  CHECK(middle->y == doctest::Approx(kHeight / 2).epsilon(kTolerance));
  // With a 90 degree view, a point as far up as it is ahead is on the top edge.
  const auto top = to_view(camera, {0, 10, -10});
  REQUIRE(top);
  CHECK(top->y == doctest::Approx(0.0f).epsilon(kTolerance));
  const auto right = to_view(camera, {3, 0, -10});
  REQUIRE(right);
  CHECK(right->x > kWidth / 2);
}

TEST_CASE("a point behind the camera shows nowhere") {
  CHECK_FALSE(to_view(looking_ahead(), {0, 0, 5}));
  CHECK_FALSE(to_view(looking_ahead(), Vec3::zero()));
  ViewCamera flat = looking_ahead();
  flat.width = 0.0f;
  CHECK_FALSE(to_view(flat, {0, 0, -10}));
}

TEST_CASE("a rectangle is the same whichever way it was dragged, and holds its edges") {
  const editor_logic::Rect rect = rect_between({50, 80}, {10, 20});
  CHECK(rect.min_x == 10);
  CHECK(rect.min_y == 20);
  CHECK(rect.max_x == 50);
  CHECK(rect.max_y == 80);
  CHECK(contains(rect, {10, 20}));
  CHECK(contains(rect, {30, 50}));
  CHECK_FALSE(contains(rect, {9, 50}));
  CHECK_FALSE(contains(rect, {30, 81}));
}

TEST_CASE("a small move of the mouse is a click, a longer one a rectangle") {
  CHECK_FALSE(dragged_far_enough({100, 100}, {102, 101}));
  CHECK(dragged_far_enough({100, 100}, {110, 101}));
  CHECK(dragged_far_enough({100, 100}, {100, 90}));
}

TEST_CASE("a rectangle selects the objects it holds, not the ones behind the camera") {
  const std::vector<Candidate> candidates = {
      {1, ViewPoint{20, 20}}, {2, ViewPoint{200, 200}}, {3, std::nullopt}, {4, ViewPoint{40, 60}}};
  CHECK(inside(candidates, rect_between({0, 0}, {100, 100})) == std::vector<int>{1, 4});
  CHECK(inside(candidates, rect_between({500, 500}, {600, 600})).empty());
}
