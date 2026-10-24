// UI natives: what scripts draw over the game.

#include <mruby.h>
#include <mruby/string.h>

#include "core/safe_path.hpp"

#include "scene/scene_config.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kOpaque = config::kMaxColorByte;  // a color channel at its fullest

// Where something is drawn, as mruby hands it over.
struct Box {
  mrb_int x = 0, y = 0, width = 0, height = 0;
};

void place(HudItem& item, const Box& box) {
  item.x = static_cast<int>(box.x);
  item.y = static_cast<int>(box.y);
  item.width = static_cast<int>(box.width);
  item.height = static_cast<int>(box.height);
}

mrb_value ui_rect(mrb_state* mrb, mrb_value) {
  Box box;
  mrb_int r = 0, g = 0, b = 0, a = 0;
  mrb_float radius = 0.0;
  mrb_get_args(mrb, "iiiiiiiif", &box.x, &box.y, &box.width, &box.height, &r, &g, &b, &a, &radius);
  radius = at_least(mrb, radius, 0.0, "a box's corner radius");
  HudItem item;
  item.kind = HudItem::Kind::Rect;
  place(item, box);
  item.radius = static_cast<float>(radius);
  item.color = color_from(r, g, b, a);
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

mrb_value ui_image(mrb_state* mrb, mrb_value) {
  const char* name = nullptr;
  Box box;
  mrb_int a = 0;
  mrb_get_args(mrb, "ziiiii", &name, &box.x, &box.y, &box.width, &box.height, &a);
  HudItem item;
  item.kind = HudItem::Kind::Image;
  item.text = name;
  place(item, box);
  item.color = color_from(kOpaque, kOpaque, kOpaque, a);
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

mrb_value ui_text(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_int x = 0, y = 0, size = 0, r = 0, g = 0, b = 0, a = 0;
  mrb_get_args(mrb, "ziiiiiii", &text, &x, &y, &size, &r, &g, &b, &a);
  HudItem hud;
  hud.text = text;
  hud.font = context(mrb).ui_font;
  hud.x = static_cast<int>(x);
  hud.y = static_cast<int>(y);
  hud.size = static_cast<int>(size);
  hud.color = color_from(r, g, b, a);
  context(mrb).hud.push_back(std::move(hud));
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

mrb_value ui_width(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).screen_width); }
mrb_value ui_height(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).screen_height); }

mrb_value ui_text_width(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_int size = 0;
  mrb_get_args(mrb, "zi", &text, &size);
  const ScriptContext& ctx = context(mrb);
  // Without anyone to measure it (a context with no engine around it): a usual width per letter.
  const int width = ctx.measure_text ? ctx.measure_text(text, static_cast<int>(size))
                                     : estimated_text_width(text, static_cast<int>(size));
  return mrb_int_value(mrb, width);
}

}  // namespace

void install_ui_bindings(mrb_state* mrb) {
  RClass* ui_module = mrb_define_module(mrb, "UI");
  mrb_define_module_function(mrb, ui_module, "font", ui_font, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "font=", ui_set_font, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, ui_module, "__text", ui_text, MRB_ARGS_REQ(8));
  mrb_define_module_function(mrb, ui_module, "__rect", ui_rect, MRB_ARGS_REQ(9));
  mrb_define_module_function(mrb, ui_module, "__image", ui_image, MRB_ARGS_REQ(6));
  mrb_define_module_function(mrb, ui_module, "__text_width", ui_text_width, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, ui_module, "width", ui_width, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "height", ui_height, MRB_ARGS_NONE());
}

}  // namespace redgem
