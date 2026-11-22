// UI natives: what scripts draw over the game.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include <nlohmann/json.hpp>

#include "core/game_files.hpp"
#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "render/font_chain.hpp"

#include "scene/scene_config.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr int kSmallestTextPixels = 1;
constexpr float kUnitSlack = 1e-3f;  // a width a hair over a whole unit, from dividing by the scale, is that unit
// How a picture UI.image draws may be named, after its name (as the renderer looks for it).
constexpr const char* kPictureExtensions[] = {"", ".png", ".jpg", ".jpeg", ".webp"};
// The largest picture whose size UI.image_size reads (its whole file is read).
constexpr std::uintmax_t kMostPictureBytes = 64u * 1024u * 1024u;

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

// A color's red, green, blue and alpha, as mruby hands them over.
struct Channels {
  mrb_int r = 0, g = 0, b = 0, a = 0;
  Color color() const { return color_from(r, g, b, a); }
};

// A filled rectangle that goes from one color at its top to another at its bottom.
mrb_value ui_gradient(mrb_state* mrb, mrb_value) {
  Box box;
  Channels top, bottom;
  mrb_get_args(mrb, "ffffiiiiiiii", &box.x, &box.y, &box.width, &box.height, &top.r, &top.g, &top.b, &top.a, &bottom.r,
               &bottom.g, &bottom.b, &bottom.a);
  HudItem item;
  item.kind = HudItem::Kind::Gradient;
  place(item, box, ui_scale_of(context(mrb)));
  item.color = top.color();
  item.bottom = bottom.color();
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

// The part of a picture to draw, in its own pixels, as mruby hands it over (none wide: all of it).
struct Part {
  mrb_float x = 0, y = 0, width = 0, height = 0;
  PicturePart picture() const {
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)};
  }
};

HudItem image_item(mrb_state* mrb, const char* name, const Box& box) {
  HudItem item;
  item.kind = HudItem::Kind::Image;
  item.text = name;
  item.scale = ui_scale_of(context(mrb));
  place(item, box, item.scale);
  return item;
}

// A picture (or a part of it), tinted with a color.
mrb_value ui_image(mrb_state* mrb, mrb_value) {
  const char* name = nullptr;
  Box box;
  Channels tint;
  Part part;
  mrb_get_args(mrb, "zffffiiiiffff", &name, &box.x, &box.y, &box.width, &box.height, &tint.r, &tint.g, &tint.b, &tint.a,
               &part.x, &part.y, &part.width, &part.height);
  HudItem item = image_item(mrb, name, box);
  item.part = part.picture();
  item.color = tint.color();
  context(mrb).hud.push_back(std::move(item));
  return mrb_nil_value();
}

// The size a picture of the game's images/ folder says it has, read from its file's first bytes.
std::optional<ImageSize> picture_size(const std::filesystem::path& folder, const std::string& name) {
  if (folder.empty()) return std::nullopt;
  const auto file = find_asset_file(folder, name, kPictureExtensions);
  const auto bytes = file ? game_files::read(*file, kMostPictureBytes) : std::nullopt;
  if (!bytes) return std::nullopt;
  return encoded_image_size(bytes->data(), bytes->size());
}

// [width, height] of a picture of images/, in its own pixels; nil when there is none.
mrb_value ui_image_size(mrb_state* mrb, mrb_value) {
  const auto size = picture_size(context(mrb).images_dir, to_name(mrb, single_arg(mrb)));
  if (!size) return mrb_nil_value();
  const mrb_value sides[] = {mrb_int_value(mrb, size->width), mrb_int_value(mrb, size->height)};
  return mrb_ary_new_from_values(mrb, static_cast<mrb_int>(std::size(sides)), sides);
}

// game.json's "menu" section, its keys as texts (GameMenu reads them against the names it knows);
// nil for a game without one.
mrb_value ui_menu_settings(mrb_state* mrb, mrb_value) {
  const std::string& text = context(mrb).menu_settings;
  if (text.empty()) return mrb_nil_value();
  return value_from(mrb, nlohmann::json::parse(text, nullptr, false), HashKeys::Strings);
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
  mrb_define_module_function(mrb, ui_module, "__image_size", ui_image_size, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, ui_module, "__menu_settings", ui_menu_settings, MRB_ARGS_NONE());
}

}  // namespace

void install_ui_bindings(mrb_state* mrb) {
  RClass* ui_module = mrb_define_module(mrb, "UI");
  mrb_define_module_function(mrb, ui_module, "font", ui_font, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "font=", ui_set_font, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, ui_module, "__text", ui_text, MRB_ARGS_REQ(8));
  mrb_define_module_function(mrb, ui_module, "__rect", ui_rect, MRB_ARGS_REQ(9));
  mrb_define_module_function(mrb, ui_module, "__image", ui_image, MRB_ARGS_REQ(13));
  mrb_define_module_function(mrb, ui_module, "__gradient", ui_gradient, MRB_ARGS_REQ(12));
  mrb_define_module_function(mrb, ui_module, "__clip", ui_clip, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, ui_module, "__unclip", ui_unclip, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, ui_module, "__text_width", ui_text_width, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, ui_module, "__text_field_drawn", ui_text_field_drawn, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, ui_module, "keyboard_visible?", ui_keyboard_visible, MRB_ARGS_NONE());
  install_ui_size_bindings(mrb, ui_module);
}

}  // namespace redgem
