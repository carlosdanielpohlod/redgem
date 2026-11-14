#include <doctest/doctest.h>

#include "app/debug_labels.hpp"

using namespace redgem;

namespace {
constexpr int kWidth = 640;
constexpr int kHeight = 360;
constexpr int kTextWidth = 40;
constexpr float kDistance = 5.0f;

LabelView view() {
  LabelView seen;
  seen.camera.position = {1.0f, 2.0f, 3.0f};
  seen.frame = {kWidth, kHeight, 0, 0};
  return seen;
}
DebugLabel label_at(Vec3 at) {
  DebugLabel label;
  label.text = "hp";
  label.at = at;
  label.size = 20;
  label.color = {1.0f, 1.0f, 0.0f, 1.0f};
  return label;
}
}  // namespace

TEST_CASE("a label straight ahead of the camera is centered on the screen") {
  const LabelView seen = view();
  const Vec3 ahead = seen.camera.position + seen.camera.forward() * kDistance;
  const auto item = debug_label_item(label_at(ahead), seen, kTextWidth);
  REQUIRE(item.has_value());
  CHECK(item->kind == HudItem::Kind::Text);
  CHECK(item->text == "hp");
  CHECK(item->x == kWidth / 2 - kTextWidth / 2);
  CHECK(item->y == kHeight / 2 - 10);
  CHECK(item->size == 20);
  CHECK(item->color.b == 0.0f);
}

TEST_CASE("a label is placed where world_to_screen shows its point") {
  const LabelView seen = view();
  const Vec3 point = seen.camera.position + seen.camera.forward() * kDistance + seen.camera.right();
  const auto item = debug_label_item(label_at(point), seen, kTextWidth);
  const auto shown = world_to_screen(seen.camera, seen.lens, seen.frame, point);
  REQUIRE(item.has_value());
  REQUIRE(shown.has_value());
  CHECK(shown->x > kWidth / 2);
  CHECK(item->x == doctest::Approx(shown->x - kTextWidth / 2).epsilon(0.01));
}

TEST_CASE("a label behind the camera, or on a screen of no size, is not shown") {
  LabelView seen = view();
  const Vec3 behind = seen.camera.position - seen.camera.forward() * kDistance;
  CHECK_FALSE(debug_label_item(label_at(behind), seen, kTextWidth).has_value());
  seen.frame = {};
  const Vec3 ahead = seen.camera.position + seen.camera.forward() * kDistance;
  CHECK_FALSE(debug_label_item(label_at(ahead), seen, kTextWidth).has_value());
}
