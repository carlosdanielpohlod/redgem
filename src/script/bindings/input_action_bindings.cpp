// Input natives for a game's own actions and axes: declaring them, and what each player binds
// them to (rebinding, and catching the next key or button pressed for a "press a key" screen).
// The Ruby side, with its keyword arguments, is ruby/engine/input.rb.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>

#include <cstddef>
#include <optional>
#include <string>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr const char* kUnknownName = "unknown action or axis :%s";
constexpr const char* kUnknownBinding = "unknown binding \"%s\" (bindings read like \"key:space\" or \"pad:a\")";
constexpr const char* kSideNeeded = "an axis is bound with side: :negative or :positive, and an action without one";
constexpr const char* kNegativeSide = "negative";
constexpr const char* kPositiveSide = "positive";

InputState& input(mrb_state* mrb) { return *context(mrb).input; }
PlayerBindings& bindings(mrb_state* mrb) { return input(mrb).bindings(); }

// Tells whoever keeps the players' bindings that they changed (nobody, outside a running game).
void changed(mrb_state* mrb) {
  const auto& listener = context(mrb).on_bindings_changed;
  if (listener) listener();
}

InputBinding binding_from(mrb_state* mrb, mrb_value text) {
  const std::string written = to_name(mrb, text);
  return known(mrb, binding_from_text(written), kUnknownBinding, written);
}

ActionBinding bindings_from(mrb_state* mrb, mrb_value list) {
  ActionBinding binding;
  for (mrb_int i = 0; i < RARRAY_LEN(list); ++i) binding.add(binding_from(mrb, mrb_ary_ref(mrb, list, i)));
  return binding;
}

std::optional<AxisSide> side_from(mrb_state* mrb, mrb_value side) {
  if (mrb_nil_p(side)) return std::nullopt;
  const std::string name = to_name(mrb, side);
  if (name == kNegativeSide) return AxisSide::Negative;
  if (name == kPositiveSide) return AxisSide::Positive;
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown side :%s (use :negative or :positive)", name.c_str());
  return std::nullopt;
}

// The name of an action or an axis the game declared.
std::string declared_name(mrb_state* mrb, mrb_value name) {
  const std::string text = to_name(mrb, name);
  const bool declared = bindings(mrb).is_action(text) || bindings(mrb).is_axis(text);
  if (!declared) mrb_raisef(mrb, E_ARGUMENT_ERROR, kUnknownName, text.c_str());
  return text;
}

mrb_value list_value(mrb_state* mrb, const ActionBinding& binding) {
  const mrb_value list = mrb_ary_new(mrb);
  for (const InputBinding& one : binding.all()) mrb_ary_push(mrb, list, text_value(mrb, binding_text(one)));
  return list;
}

// The names of a map's actions or axes, as Symbols.
template <typename Named>
mrb_value names_value(mrb_state* mrb, const Named& named) {
  const mrb_value names = mrb_ary_new(mrb);
  for (const auto& entry : named) mrb_ary_push(mrb, names, mrb_symbol_value(mrb_intern_cstr(mrb, entry.first.c_str())));
  return names;
}

// --- Declaring -------------------------------------------------------------------------------

mrb_value input_define_action(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_value list;
  mrb_get_args(mrb, "oA", &name, &list);
  bindings(mrb).declare_action(to_name(mrb, name), bindings_from(mrb, list));
  return mrb_nil_value();
}

mrb_value input_define_axis(mrb_state* mrb, mrb_value) {
  mrb_value name, negative, positive, stick;
  mrb_bool invert = false;
  mrb_get_args(mrb, "oAAob", &name, &negative, &positive, &stick, &invert);
  AxisBinding axis{bindings_from(mrb, negative), bindings_from(mrb, positive), std::nullopt, invert != 0};
  if (!mrb_nil_p(stick)) {
    const std::string stick_name = to_name(mrb, stick);
    axis.stick = known(mrb, gamepad_axis_from_name(stick_name), "unknown gamepad axis :%s", stick_name);
  }
  bindings(mrb).declare_axis(to_name(mrb, name), std::move(axis));
  return mrb_nil_value();
}

mrb_value input_actions(mrb_state* mrb, mrb_value) { return names_value(mrb, bindings(mrb).declared().actions()); }
mrb_value input_axes(mrb_state* mrb, mrb_value) { return names_value(mrb, bindings(mrb).declared().axes()); }

// --- Rebinding -------------------------------------------------------------------------------

// What bind and unbind are given: a name, a binding (nil: all of them), a side and a player.
struct BindArgs {
  std::string name;
  std::optional<InputBinding> binding;
  std::optional<AxisSide> side;
  std::size_t pad = kFirstGamepad;
};

BindArgs bind_args(mrb_state* mrb) {
  mrb_value name, text, side;
  mrb_int pad = 0;
  mrb_get_args(mrb, "oooi", &name, &text, &side, &pad);
  BindArgs args{declared_name(mrb, name), std::nullopt, side_from(mrb, side), pad_number(mrb, pad)};
  if (!mrb_nil_p(text)) args.binding = binding_from(mrb, text);
  return args;
}

mrb_value input_bind(mrb_state* mrb, mrb_value) {
  const BindArgs args = bind_args(mrb);
  if (!args.binding) mrb_raise(mrb, E_ARGUMENT_ERROR, "say what to bind: key:, button: or a binding");
  if (!bindings(mrb).bind(args.pad, args.name, *args.binding, args.side)) mrb_raise(mrb, E_ARGUMENT_ERROR, kSideNeeded);
  changed(mrb);
  return mrb_true_value();
}

mrb_value input_unbind(mrb_state* mrb, mrb_value) {
  const BindArgs args = bind_args(mrb);
  if (!bindings(mrb).unbind(args.pad, args.name, args.binding, args.side)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, kSideNeeded);
  }
  changed(mrb);
  return mrb_true_value();
}

mrb_value input_bindings(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_int pad = 0;
  mrb_get_args(mrb, "oi", &name, &pad);
  const std::string text = declared_name(mrb, name);
  const std::size_t player = pad_number(mrb, pad);
  if (const ActionBinding* action = bindings(mrb).action(player, text)) return list_value(mrb, *action);
  const AxisBinding& axis = *bindings(mrb).axis(player, text);
  const mrb_value sides = mrb_hash_new(mrb);
  set_entry(mrb, sides, kNegativeSide, list_value(mrb, axis.negative));
  set_entry(mrb, sides, kPositiveSide, list_value(mrb, axis.positive));
  return sides;
}

// Back to the game's bindings: one name or all, for one player or all (nil for either: all).
mrb_value input_reset_bindings(mrb_state* mrb, mrb_value) {
  mrb_value name, pad;
  mrb_get_args(mrb, "oo", &name, &pad);
  std::optional<std::string> which;
  if (!mrb_nil_p(name)) which = declared_name(mrb, name);
  const std::size_t first = mrb_nil_p(pad) ? kFirstGamepad : pad_number(mrb, mrb_as_int(mrb, pad));
  const std::size_t end = mrb_nil_p(pad) ? kMaxGamepads : first + 1;
  for (std::size_t player = first; player < end; ++player) bindings(mrb).reset(player, which);
  changed(mrb);
  return mrb_nil_value();
}

// --- Capturing the next key or button ----------------------------------------------------------

mrb_value input_capture(mrb_state* mrb, mrb_value) {
  input(mrb).capture_next();
  return mrb_nil_value();
}

mrb_value input_cancel_capture(mrb_state* mrb, mrb_value) {
  input(mrb).cancel_capture();
  return mrb_nil_value();
}

mrb_value input_capturing(mrb_state* mrb, mrb_value) { return mrb_bool_value(input(mrb).capturing()); }

void install_declaring(RClass* input_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, input_module, "__define_action", input_define_action, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__define_axis", input_define_axis, MRB_ARGS_REQ(5));
  mrb_define_module_function(mrb, input_module, "actions", input_actions, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "axes", input_axes, MRB_ARGS_NONE());
}

void install_rebinding(RClass* input_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, input_module, "__bind", input_bind, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, input_module, "__unbind", input_unbind, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, input_module, "__bindings", input_bindings, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__reset_bindings", input_reset_bindings, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, input_module, "__capture", input_capture, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "__cancel_capture", input_cancel_capture, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, input_module, "capturing?", input_capturing, MRB_ARGS_NONE());
}

}  // namespace

void install_input_action_bindings(mrb_state* mrb) {
  RClass* input_module = mrb_define_module(mrb, "Input");
  install_declaring(input_module, mrb);
  install_rebinding(input_module, mrb);
}

}  // namespace redgem
