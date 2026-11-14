// UI natives: what scripts draw over the game.

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "core/safe_path.hpp"
#include "render/font_chain.hpp"

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

// What follows is drawn only inside a box (one with no area hides it all).
mrb_value ui_clip(mrb_state* mrb, mrb_value) {
  Box box;
  mrb_get_args(mrb, "iiii", &box.x, &box.y, &box.width, &box.height);
  HudItem item;
  item.kind = HudItem::Kind::Clip;
  place(item, box);
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

constexpr const char* kFontRefusal = "UI.font must name a file in fonts/, or a list of up to %d of them";

[[noreturn]] void refuse_font(mrb_state* mrb) {
  mrb_raisef(mrb, E_ARGUMENT_ERROR, kFontRefusal, static_cast<int>(kMaxChainFaces));
}

bool valid_font_name(const std::string& name) { return !name.empty() && safe_asset_name(name); }
std::string font_name(mrb_state* mrb, mrb_value value) {
  std::string name = to_name(mrb, value);
  if (!valid_font_name(name)) refuse_font(mrb);
  return name;
}
// A list of faces, the first that has a letter drawing it (render/font_chain.hpp).
std::string chain_of(mrb_state* mrb, mrb_value list) {
  const mrb_int count = RARRAY_LEN(list);
  if (static_cast<std::size_t>(count) > kMaxChainFaces) refuse_font(mrb);
  std::vector<std::string> names;
  names.reserve(static_cast<std::size_t>(count));
  for (mrb_int i = 0; i < count; ++i) names.push_back(font_name(mrb, mrb_ary_ref(mrb, list, i)));
  return font_chain(names);
}
bool empty_string(mrb_value value) { return mrb_string_p(value) && RSTRING_LEN(value) == 0; }
bool builtin_face(mrb_value value) { return mrb_nil_p(value) || empty_string(value); }

mrb_value ui_font(mrb_state* mrb, mrb_value) {
  const std::vector<std::string> names = font_chain_names(context(mrb).ui_font);
  if (names.empty()) return mrb_nil_value();
  if (names.size() == 1) return mrb_str_new_cstr(mrb, names.front().c_str());
  mrb_value list = mrb_ary_new_capa(mrb, static_cast<mrb_int>(names.size()));
  for (const std::string& name : names) mrb_ary_push(mrb, list, mrb_str_new_cstr(mrb, name.c_str()));
  return list;
}
// A face's name, a list of them (a fallback chain), or nil (or "") for the built-in face.
mrb_value ui_set_font(mrb_state* mrb, mrb_value) {
  const auto value = single_arg(mrb);
  if (builtin_face(value))
    context(mrb).ui_font.clear();
  else
    context(mrb).ui_font = mrb_array_p(value) ? chain_of(mrb, value) : font_name(mrb, value);
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
  mrb_define_module_function(mrb, ui_module, "width", ui_width, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "height", ui_height, MRB_ARGS_NONE());
}

}  // namespace redgem
