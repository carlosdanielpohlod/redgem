// A HUD clip (UI.clip) cuts what is drawn after it to its box, until it ends; one left on at the end
// of the HUD ends there.
#include <doctest/doctest.h>

#include <vector>

#include "render_system/render_stage.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr int kSide = 256;                // the stage's picture, across and down
constexpr int kHalf = kSide / 2;          // the clip: the picture's top-left quarter
constexpr float kInside = 0.25f;          // a share of the picture inside that quarter, either way
constexpr float kOutside = 0.75f;         // and one past it
constexpr float kBelow = 0.6f;            // down past the quarter, above the strip below
constexpr float kBottomStrip = 0.9f;      // down in the strip drawn after the clip ends
constexpr int kStripTop = kSide * 3 / 4;  // where that strip starts
constexpr unsigned char kBright = 150;
constexpr unsigned char kDark = 70;

bool red(::Color c) { return c.r > kBright && c.g < kDark && c.b < kDark; }
bool blue(::Color c) { return c.b > kBright && c.r < kDark && c.g < kDark; }
bool white(::Color c) { return c.r > kBright && c.g > kBright && c.b > kBright; }

HudItem rect(int y, int height, redgem::Color color) {
  HudItem item;
  item.kind = HudItem::Kind::Rect;
  item.y = y;
  item.width = kSide;
  item.height = height;
  item.color = color;
  return item;
}

HudItem clip(int width, int height) {
  HudItem item;
  item.kind = HudItem::Kind::Clip;
  item.width = width;
  item.height = height;
  return item;
}

HudItem unclip() {
  HudItem item;
  item.kind = HudItem::Kind::Unclip;
  return item;
}

const redgem::Color kRed{1.0f, 0.0f, 0.0f, 1.0f};
const redgem::Color kBlue{0.0f, 0.0f, 1.0f, 1.0f};

}  // namespace

TEST_CASE("a HUD clip cuts what is drawn inside it to its box, and what follows its end is whole") {
  RenderStage stage;
  const std::vector<HudItem> hud{clip(kHalf, kHalf), rect(0, kSide, kRed), unclip(),
                                 rect(kStripTop, kSide - kStripTop, kBlue)};
  const Picture picture = stage.draw_hud(hud, "hud_clip");
  CHECK(red(picture.at(kInside, kInside)));
  CHECK(white(picture.at(kOutside, kInside)));      // cut off on the right
  CHECK(white(picture.at(kInside, kBelow)));        // and below
  CHECK(blue(picture.at(kOutside, kBottomStrip)));  // drawn after the clip, across the whole picture
}

TEST_CASE("a HUD clip left on ends with the HUD") {
  RenderStage stage;
  stage.draw_hud({clip(kHalf, kHalf)}, "hud_clip_left_on");
  const Picture picture = stage.draw_hud({rect(0, kSide, kRed)}, "hud_clip_next_frame");
  CHECK(red(picture.at(kOutside, kOutside)));
}
