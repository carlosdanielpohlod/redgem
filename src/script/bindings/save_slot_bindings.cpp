#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <exception>
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
mrb_value select(mrb_state* mrb, mrb_value) {
  const auto name = name_arg(mrb);
  if (!valid_save_slot(name))
    mrb_raise(mrb, E_ARGUMENT_ERROR,
              "slot name must be 1..48 lowercase letters/digits, '-' or '_', starting with a letter/digit");
  auto& ctx = slots(mrb);
  try {
    ctx.select_save_slot(name);
  } catch (const std::exception& error) {
    mrb_raise(mrb, E_RUNTIME_ERROR, error.what());
  }
  return mrb_str_new(mrb, name.data(), static_cast<mrb_int>(name.size()));
}
mrb_value names(mrb_state* mrb, mrb_value) {
  auto& ctx = slots(mrb);
  std::vector<std::string> values;
  try {
    values = ctx.list_save_slots();
  } catch (const std::exception& error) {
    mrb_raise(mrb, E_RUNTIME_ERROR, error.what());
  }
  auto result = mrb_ary_new(mrb);
  for (const auto& name : values) mrb_ary_push(mrb, result, mrb_str_new_cstr(mrb, name.c_str()));
  return result;
}
}  // namespace
void install_save_slot_bindings(mrb_state* mrb) {
  auto* save = mrb_module_get(mrb, "Save");
  mrb_define_module_function(mrb, save, "slot", current, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, save, "slot=", select, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, save, "slots", names, MRB_ARGS_NONE());
}
}  // namespace redgem
