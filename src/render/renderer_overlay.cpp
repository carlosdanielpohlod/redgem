// What is drawn flat over the picture: the game's texts, rectangles and images, and a touch
// screen's controls.

#include "render/renderer_impl.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace redgem {
namespace {

using render_math::to_raylib;

// On-screen touch controls: white, see-through, more solid under a finger.
constexpr unsigned char kTouchShade = config::kMaxColorByte;
constexpr unsigned char kTouchBaseAlpha = 28;
constexpr unsigned char kTouchRestingAlpha = 70;
constexpr unsigned char kTouchPressedAlpha = 150;
constexpr float kUiLetterSpacing = 0.0f;  // extra space between letters, as a share of the text size
constexpr int kUiCornerSegments = 8;      // straight pieces each rounded corner is drawn with

Rectangle box_of(const HudItem& item) {
  return {static_cast<float>(item.x), static_cast<float>(item.y), static_cast<float>(item.width),
          static_cast<float>(item.height)};
}

void draw_hud_rect(const HudItem& item) {
  const Rectangle box = box_of(item);
  const float roundness = render_math::corner_roundness(item.radius, box.width, box.height);
  if (roundness > render_math::kSquareCorners) {
    DrawRectangleRounded(box, roundness, kUiCornerSegments, to_raylib(item.color));
  } else {
    DrawRectangleRec(box, to_raylib(item.color));
  }
}

bool is_clip(HudItem::Kind kind) { return kind == HudItem::Kind::Clip || kind == HudItem::Kind::Unclip; }

// Cuts what follows to the item's box (Clip), or stops cutting it (Unclip); returns whether it is cut.
bool clip_hud(const HudItem& item) {
  if (item.kind == HudItem::Kind::Unclip) {
    EndScissorMode();
    return false;
  }
  BeginScissorMode(item.x, item.y, item.width, item.height);
  return true;
}

unsigned char control_alpha(ControlCircle::Look look) {
  if (look == ControlCircle::Look::Base) return kTouchBaseAlpha;
  return look == ControlCircle::Look::Pressed ? kTouchPressedAlpha : kTouchRestingAlpha;
}

}  // namespace

void Renderer::Impl::draw_hud_text(const HudItem& item) {
  const float size = static_cast<float>(item.size);
  DrawTextEx(ui_fonts.get(item.font, item.text), item.text.c_str(),
             {static_cast<float>(item.x), static_cast<float>(item.y)}, size, size * kUiLetterSpacing,
             to_raylib(item.color));
}

void Renderer::Impl::draw_hud_image(const HudItem& item) {
  const Texture2D* picture = ui_image(item.text);
  if (picture == nullptr) return;
  const render_math::Size size = render_math::image_size(item.width, item.height, picture->width, picture->height);
  DrawTexturePro(*picture, {0.0f, 0.0f, static_cast<float>(picture->width), static_cast<float>(picture->height)},
                 {static_cast<float>(item.x), static_cast<float>(item.y), size.width, size.height}, {0.0f, 0.0f}, 0.0f,
                 to_raylib(item.color));
}

void Renderer::Impl::draw_hud_item(const HudItem& item) {
  switch (item.kind) {
    case HudItem::Kind::Text:
      draw_hud_text(item);
      break;
    case HudItem::Kind::Rect:
      draw_hud_rect(item);
      break;
    case HudItem::Kind::Image:
      draw_hud_image(item);
      break;
    case HudItem::Kind::Clip:
    case HudItem::Kind::Unclip:
      break;
  }
}

// A clip still on at the end (a script that never ended it) ends there: nothing after the HUD is cut.
void Renderer::draw_hud(const std::vector<HudItem>& hud) {
  bool clipped = false;
  for (const HudItem& item : hud) {
    if (is_clip(item.kind)) {
      clipped = clip_hud(item);
    } else {
      impl_->draw_hud_item(item);
    }
  }
  if (clipped) EndScissorMode();
}

int Renderer::text_width(const std::string& text, int size) const {
  const float text_size = static_cast<float>(size);
  return static_cast<int>(
      std::ceil(MeasureTextEx(impl_->ui_fonts.get("", text), text.c_str(), text_size, text_size * kUiLetterSpacing).x));
}

int Renderer::font_text_width(const std::string& text, int size, const std::string& font,
                              const std::filesystem::path& folder) const {
  impl_->ui_fonts.set_directory(folder);
  const float pixels = static_cast<float>(size);
  return static_cast<int>(
      std::ceil(MeasureTextEx(impl_->ui_fonts.get(font, text), text.c_str(), pixels, pixels * kUiLetterSpacing).x));
}

void Renderer::draw_controls(const std::vector<ControlCircle>& controls) {
  for (const ControlCircle& circle : controls) {
    const ::Color fill{kTouchShade, kTouchShade, kTouchShade, control_alpha(circle.look)};
    DrawCircleV({circle.x, circle.y}, circle.radius, fill);
  }
}

}  // namespace redgem
