#include <mruby.h>
#include <mruby/string.h>

#include <string>

#include "render/debug_draw.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {
using namespace binding;
constexpr int kNativeDrawArgs = 7;
constexpr int kNativeLabelArgs = 7;
// (The limits of debug_draw.hpp: kMaxDebugLabelSize and kMaxDebugLabelBytes.)
constexpr const char* kInvalidLabel = "debug label needs a finite position, a size of 1 to 256 and at most 256 bytes";
struct DrawArgs {
  mrb_value first{}, second{};
  mrb_int r = 0, g = 0, b = 0, a = 0;
  mrb_bool depth = true;
};
Color color(mrb_state* mrb, const DrawArgs& args) {
  return {channel_from(mrb, static_cast<mrb_float>(args.r), "debug red"),
          channel_from(mrb, static_cast<mrb_float>(args.g), "debug green"),
          channel_from(mrb, static_cast<mrb_float>(args.b), "debug blue"),
          channel_from(mrb, static_cast<mrb_float>(args.a), "debug alpha")};
}
mrb_value draw(mrb_state* mrb, DebugDrawItem::Kind kind) {
  DrawArgs args;
  mrb_get_args(mrb, "ooiiiib", &args.first, &args.second, &args.r, &args.g, &args.b, &args.a, &args.depth);
  const DebugDrawItem item{kind, to_vec3(mrb, args.first), to_vec3(mrb, args.second), color(mrb, args), args.depth};
  if (!valid_debug_draw(item))
    mrb_raise(mrb, E_ARGUMENT_ERROR, "debug shape must have finite bounds and nonnegative size");
  auto* buffer = context(mrb).debug_draw;
  return mrb_bool_value(buffer != nullptr && buffer->append(item));
}
struct LabelArgs {
  char* text = nullptr;
  mrb_int length = 0;
  mrb_value at{};
  mrb_int r = 0, g = 0, b = 0, a = 0, size = 0;
};
// A size out of range is made one that is refused (cast as it is, it could wrap into the range).
int label_size(mrb_int size) {
  const bool in_range = size >= kMinDebugLabelSize && size <= kMaxDebugLabelSize;
  return in_range ? static_cast<int>(size) : 0;
}
DebugLabel label_from(mrb_state* mrb, const LabelArgs& args) {
  const DrawArgs channels{mrb_nil_value(), mrb_nil_value(), args.r, args.g, args.b, args.a, true};
  return {std::string(args.text, static_cast<std::size_t>(args.length)), to_vec3(mrb, args.at), color(mrb, channels),
          label_size(args.size)};
}
mrb_value label(mrb_state* mrb, mrb_value) {
  LabelArgs args;
  mrb_get_args(mrb, "soiiiii", &args.text, &args.length, &args.at, &args.r, &args.g, &args.b, &args.a, &args.size);
  const DebugLabel item = label_from(mrb, args);
  if (!valid_debug_label(item)) mrb_raise(mrb, E_ARGUMENT_ERROR, kInvalidLabel);
  auto* buffer = context(mrb).debug_draw;
  return mrb_bool_value(buffer != nullptr && buffer->append(item));
}
mrb_value line(mrb_state* mrb, mrb_value) { return draw(mrb, DebugDrawItem::Kind::Line); }
mrb_value box(mrb_state* mrb, mrb_value) { return draw(mrb, DebugDrawItem::Kind::Box); }
}  // namespace
void install_debug_draw_bindings(mrb_state* mrb) {
  auto* debug = mrb_module_get(mrb, "Debug");
  mrb_define_module_function(mrb, debug, "__draw_line", line, MRB_ARGS_REQ(kNativeDrawArgs));
  mrb_define_module_function(mrb, debug, "__draw_box", box, MRB_ARGS_REQ(kNativeDrawArgs));
  mrb_define_module_function(mrb, debug, "__draw_label", label, MRB_ARGS_REQ(kNativeLabelArgs));
}
}  // namespace redgem
