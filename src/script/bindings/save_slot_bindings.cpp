#include <mruby.h>
#include <mruby/array.h>
#include <mruby/error.h>
#include <mruby/string.h>
#include <exception>
#include <string>
#include <vector>
#include "storage/save_slots.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
namespace redgem {
namespace {
using namespace binding;
ScriptContext& slots(mrb_state* mrb) {
  auto& ctx = context(mrb);
  const bool available = ctx.save != nullptr && ctx.select_save_slot && ctx.list_save_slots;
  if (!available) mrb_raise(mrb, E_RUNTIME_ERROR, "Save slots are only available while a game runs");
  return ctx;
}
mrb_value current(mrb_state* mrb, mrb_value) {
  const auto& name = slots(mrb).save_slot;
  return mrb_str_new(mrb, name.data(), static_cast<mrb_int>(name.size()));
}
mrb_value try_select(mrb_state* mrb, ScriptContext& ctx, const std::string& name) {
  if (!valid_save_slot(name)) {
    constexpr char kInvalidName[] =
        "slot name must be 1..48 lowercase letters/digits, '-' or '_', starting with a letter/digit";
    return mrb_exc_new(mrb, E_ARGUMENT_ERROR, kInvalidName, sizeof(kInvalidName) - 1);
  }
  std::string failure;
  try {
    ctx.select_save_slot(name);
    return mrb_nil_value();
  } catch (const std::exception& error) {
    failure = error.what();
  }
  return mrb_exc_new(mrb, E_RUNTIME_ERROR, failure.data(), failure.size());
}
mrb_value select(mrb_state* mrb, mrb_value) {
  auto& ctx = slots(mrb);
  mrb_value error;
  {
    const auto name = name_arg(mrb);
    error = try_select(mrb, ctx, name);
    if (mrb_nil_p(error)) return mrb_str_new(mrb, name.data(), static_cast<mrb_int>(name.size()));
  }
  mrb_exc_raise(mrb, error);
}
mrb_value try_list(mrb_state* mrb, ScriptContext& ctx, std::vector<std::string>& values) {
  std::string failure;
  try {
    values = ctx.list_save_slots();
    return mrb_nil_value();
  } catch (const std::exception& error) {
    failure = error.what();
  }
  return mrb_exc_new(mrb, E_RUNTIME_ERROR, failure.data(), failure.size());
}
mrb_value list_names(mrb_state* mrb, const std::vector<std::string>& values) {
  auto result = mrb_ary_new(mrb);
  for (const auto& name : values) mrb_ary_push(mrb, result, mrb_str_new_cstr(mrb, name.c_str()));
  return result;
}
mrb_value names(mrb_state* mrb, mrb_value) {
  auto& ctx = slots(mrb);
  mrb_value error;
  {
    std::vector<std::string> values;
    error = try_list(mrb, ctx, values);
    if (mrb_nil_p(error)) return list_names(mrb, values);
  }
  mrb_exc_raise(mrb, error);
}
}  // namespace
void install_save_slot_bindings(mrb_state* mrb) {
  auto* save = mrb_module_get(mrb, "Save");
  mrb_define_module_function(mrb, save, "slot", current, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, save, "slot=", select, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, save, "slots", names, MRB_ARGS_NONE());
}
}  // namespace redgem
