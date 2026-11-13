#include <doctest/doctest.h>
#include <limits>

#include "render/debug_draw.hpp"

using namespace redgem;

TEST_CASE("debug commands remain bounded and clearing resets the drop count") {
  DebugDrawBuffer buffer;
  const DebugDrawItem line{};
  for (std::size_t i = 0; i < kMaxDebugDrawCommands; ++i) REQUIRE(buffer.append(line));
  CHECK_FALSE(buffer.append(line));
  CHECK_FALSE(buffer.append(line));
  CHECK(buffer.items().size() == kMaxDebugDrawCommands);
  CHECK(buffer.dropped() == 2);
  buffer.clear();
  CHECK(buffer.items().empty());
  CHECK(buffer.dropped() == 0);
  CHECK(buffer.append(line));
}
TEST_CASE("invalid debug geometry and colors are rejected without consuming slots") {
  DebugDrawBuffer buffer;
  DebugDrawItem item;
  item.first.x = std::numeric_limits<float>::quiet_NaN();
  CHECK_FALSE(buffer.append(item));
  item = {};
  item.color.a = std::numeric_limits<float>::infinity();
  CHECK_FALSE(buffer.append(item));
  item = {};
  item.color.r = -1.0f;
  CHECK_FALSE(buffer.append(item));
  item = {};
  item.kind = static_cast<DebugDrawItem::Kind>(-1);
  CHECK_FALSE(buffer.append(item));
  CHECK(buffer.items().empty());
  CHECK(buffer.dropped() == 0);
}
TEST_CASE("debug boxes refuse negative sizes and overflowing corners") {
  DebugDrawBuffer buffer;
  DebugDrawItem box;
  box.kind = DebugDrawItem::Kind::Box;
  box.second = {-1.0f, 1.0f, 1.0f};
  CHECK_FALSE(buffer.append(box));
  box.second = {1.0f, 0.0f, 1.0f};
  CHECK(buffer.append(box));
  box.first.x = box.second.x = std::numeric_limits<float>::max();
  CHECK_FALSE(buffer.append(box));
  CHECK(buffer.items().size() == 1);
}
TEST_CASE("debug descriptions preserve world space, color and depth intent") {
  DebugDrawBuffer buffer;
  DebugDrawItem line;
  line.first = {1.0f, 2.0f, 3.0f};
  line.second = {4.0f, 5.0f, 6.0f};
  line.color = {1.0f, 0.0f, 0.0f, 1.0f};
  line.depth_test = false;
  REQUIRE(buffer.append(line));
  CHECK(buffer.items().front().first.y == 2.0f);
  CHECK(buffer.items().front().second.z == 6.0f);
  CHECK_FALSE(buffer.items().front().depth_test);
}
TEST_CASE("debug labels need a finite point, a size in range and a short text, and are bounded") {
  DebugDrawBuffer buffer;
  DebugLabel label;
  label.text = "spawn";
  CHECK(buffer.append(label));
  label.at.y = std::numeric_limits<float>::infinity();
  CHECK_FALSE(buffer.append(label));
  label = {};
  label.size = kMaxDebugLabelSize + 1;
  CHECK_FALSE(buffer.append(label));
  label = {};
  label.text.assign(kMaxDebugLabelBytes + 1, 'x');
  CHECK_FALSE(buffer.append(label));
  CHECK(buffer.labels().size() == 1);
  label = {};
  while (buffer.labels().size() < kMaxDebugLabels) REQUIRE(buffer.append(label));
  CHECK_FALSE(buffer.append(label));
  CHECK(buffer.dropped() == 1);
  buffer.clear();
  CHECK(buffer.labels().empty());
}
