// UI natives: what scripts draw over the game.

#include <algorithm>
#include <cmath>
#include <iterator>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "core/safe_path.hpp"

#include "scene/scene_config.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kOpaque = config::kMaxColorByte;  // a color channel at its fullest

constexpr int kSmallestTextPixels = 1;
constexpr float kUnitSlack = 1e-3f;  // a width a hair over a whole unit, from dividing by the scale, is that unit

// Where something is drawn, in units of the interface (see ui_scale_of), as mruby hands it over.
struct Box {
  mrb_float x = 0, y = 0, width = 0, height = 0;
};

int pixel(mrb_float units, float scale) { return static_cast<int>(std::lround(units * scale)); }

// A box in units, in the screen's pixels: its edges each rounded to a pixel, so boxes that touch
// still touch.
void place(HudItem& item, const Box& box, float scale) {
  item.x = pixel(box.x, scale);
  item.y = pixel(box.y, scale);
  item.width = pixel(box.x + box.width, scale) - item.x;
  item.height = pixel(box.y + box.height, scale) - item.y;
}

// The pixels a text `size` units tall is drawn (and its letters made) at.
int text_pixels(mrb_int size, float scale) {
  return std::max(kSmallestTextPixels, pixel(static_cast<mrb_float>(size), scale));
}

mrb_value ui_rect(mrb_state* mrb, mrb_value) {
  Box box;
  mrb_int r = 0, g = 0, b = 0, a = 0;
  mrb_float radius = 0.0;
  mrb_get_args(mrb, "ffffiiiif", &box.x, &box.y, &box.width, &box.height, &r, &g, &b, &a, &radius);
  radius = at_least(mrb, radius, 0.0, "a box's corner radius");
  const float scale = ui_scale_of(context(mrb));
  HudItem item;
  item.kind = HudItem::Kind::Rect;
  place(item, box, scale);
  item.radius = static_cast<float>(radius) * scale;
  item.color = color_from(r, g, b, a);
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

mrb_value ui_image(mrb_state* mrb, mrb_value) {
  const char* name = nullptr;
  Box box;
  mrb_int a = 0;
  mrb_get_args(mrb, "zffffi", &name, &box.x, &box.y, &box.width, &box.height, &a);
  HudItem item;
  item.kind = HudItem::Kind::Image;
  item.text = name;
  item.scale = ui_scale_of(context(mrb));
  place(item, box, item.scale);
  item.color = color_from(kOpaque, kOpaque, kOpaque, a);
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

mrb_value ui_text(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_float x = 0, y = 0;
  mrb_int size = 0, r = 0, g = 0, b = 0, a = 0;
  mrb_get_args(mrb, "zffiiiii", &text, &x, &y, &size, &r, &g, &b, &a);
  const float scale = ui_scale_of(context(mrb));
  HudItem hud;
  hud.text = text;
  hud.font = context(mrb).ui_font;
  hud.x = pixel(x, scale);
  hud.y = pixel(y, scale);
  hud.size = text_pixels(size, scale);
  hud.color = color_from(r, g, b, a);
  context(mrb).hud.push_back(std::move(hud));
  return mrb_nil_value();
}

// What follows is drawn only inside a box (one with no area hides it all).
mrb_value ui_clip(mrb_state* mrb, mrb_value) {
  Box box;
  mrb_get_args(mrb, "ffff", &box.x, &box.y, &box.width, &box.height);
  HudItem item;
  item.kind = HudItem::Kind::Clip;
  place(item, box, ui_scale_of(context(mrb)));
  item.width = std::max(item.width, 0);
  item.height = std::max(item.height, 0);
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

// What follows is drawn anywhere again.
mrb_value ui_unclip(mrb_state* mrb, mrb_value) {
  HudItem item;
  item.kind = HudItem::Kind::Unclip;
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

bool valid_font_name(const std::string& name) { return name.empty() || safe_asset_name(name); }

mrb_value ui_font(mrb_state* mrb, mrb_value) {
  const auto& name = context(mrb).ui_font;
  return name.empty() ? mrb_nil_value() : mrb_str_new_cstr(mrb, name.c_str());
}
mrb_value ui_set_font(mrb_state* mrb, mrb_value) {
  const auto value = single_arg(mrb);
  const std::string name = mrb_nil_p(value) ? "" : to_name(mrb, value);
  if (!valid_font_name(name)) mrb_raise(mrb, E_ARGUMENT_ERROR, "UI.font must name a file in fonts/");
  context(mrb).ui_font = name;
  return value;
}

// A length of the screen's, in pixels, in units of the interface.
mrb_value in_units(mrb_state* mrb, int pixels) {
  return mrb_int_value(mrb, std::lround(static_cast<float>(pixels) / ui_scale_of(context(mrb))));
}

mrb_value ui_width(mrb_state* mrb, mrb_value) { return in_units(mrb, context(mrb).screen_width); }
mrb_value ui_height(mrb_state* mrb, mrb_value) { return in_units(mrb, context(mrb).screen_height); }
mrb_value ui_pixel_width(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).screen_width); }
mrb_value ui_pixel_height(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).screen_height); }
mrb_value ui_scale_value(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, ui_scale_of(context(mrb))); }

// [width, height] of the screen the interface is laid out for; [0, 0] when there is none.
mrb_value ui_reference(mrb_state* mrb, mrb_value) {
  const SurfaceSize size = context(mrb).ui_reference.size;
  const mrb_value sides[] = {mrb_int_value(mrb, size.width), mrb_int_value(mrb, size.height)};
  return mrb_ary_new_from_values(mrb, static_cast<mrb_int>(std::size(sides)), sides);
}

mrb_value ui_text_width(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_int size = 0;
  mrb_get_args(mrb, "zi", &text, &size);
  const ScriptContext& ctx = context(mrb);
  // Measured at the pixels it is drawn at. Without anyone to measure it (a context with no engine
  // around it): a usual width per letter.
  const int pixels = text_pixels(size, ui_scale_of(ctx));
  const int width = ctx.measure_text ? ctx.measure_text(text, pixels) : estimated_text_width(text, pixels);
  // (Rounded up: a text is never taken to be narrower than it is drawn.)
  const float units = static_cast<float>(width) / ui_scale_of(ctx) - kUnitSlack;
  return mrb_int_value(mrb, static_cast<mrb_int>(std::ceil(units)));
}

// A text field drawn this frame, tapped (or clicked) or not: a touch screen shows its keyboard
// for it.
mrb_value ui_text_field_drawn(mrb_state* mrb, mrb_value) {
  const bool tapped = bool_arg(mrb);
  TextInputRequest& request = context(mrb).text_input;
  request.field_shown = true;
  request.field_tapped = request.field_tapped || tapped;
  return mrb_nil_value();
}

mrb_value ui_keyboard_visible(mrb_state* mrb, mrb_value) { return mrb_bool_value(context(mrb).keyboard_visible); }

// The screen's size, in units and in pixels, and the interface's scale and reference.
void install_ui_size_bindings(mrb_state* mrb, RClass* ui_module) {
  mrb_define_module_function(mrb, ui_module, "width", ui_width, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "height", ui_height, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "pixel_width", ui_pixel_width, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "pixel_height", ui_pixel_height, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "scale", ui_scale_value, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "reference", ui_reference, MRB_ARGS_NONE());
}

}  // namespace

void install_ui_bindings(mrb_state* mrb) {
  RClass* ui_module = mrb_define_module(mrb, "UI");
  mrb_define_module_function(mrb, ui_module, "font", ui_font, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "font=", ui_set_font, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, ui_module, "__text", ui_text, MRB_ARGS_REQ(8));
  mrb_define_module_function(mrb, ui_module, "__rect", ui_rect, MRB_ARGS_REQ(9));
  mrb_define_module_function(mrb, ui_module, "__image", ui_image, MRB_ARGS_REQ(6));
  mrb_define_module_function(mrb, ui_module, "__clip", ui_clip, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, ui_module, "__unclip", ui_unclip, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "__text_width", ui_text_width, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, ui_module, "__text_field_drawn", ui_text_field_drawn, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, ui_module, "keyboard_visible?", ui_keyboard_visible, MRB_ARGS_NONE());
  install_ui_size_bindings(mrb, ui_module);
}

}  // namespace redgem
