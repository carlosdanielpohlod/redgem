#include <doctest/doctest.h>

#include "core/frame_budget.hpp"

using namespace redgem;

namespace {
constexpr double kBudget = 0.004;
constexpr double kHalf = 0.002;
}  // namespace

TEST_CASE("a frame budget is spent once its clock has gone past its share") {
  double now = 1.0;
  const FrameBudget budget([&] { return now; }, kBudget);
  CHECK_FALSE(budget.spent());
  now += kHalf;
  CHECK(budget.used() == doctest::Approx(kHalf));
  CHECK_FALSE(budget.spent());
  now += kHalf;
  CHECK(budget.spent());
}

TEST_CASE("an unlimited frame budget is never spent") {
  double now = 0.0;
  const FrameBudget budget([&] { return now; }, FrameBudget::kUnlimited);
  now += 1.0e6;
  CHECK_FALSE(budget.spent());
}
