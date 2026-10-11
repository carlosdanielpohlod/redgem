// Save natives: what a game keeps between runs.

#include <cmath>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "app/save_store.hpp"
#include "core/log.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr double kLargestSavedInteger = 9007199254740992.0;  // 2^53: whole numbers a double holds exactly

// --- Save --------------------------------------------------------------------------------------

SaveStore& save(mrb_state* mrb) {
  SaveStore* store = context(mrb).save;
  if (store == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "Save is only available while a game runs");
  return *store;
}

// Whole numbers come back as Integers: a saved count of 3 is 3, not 3.0.
mrb_value number_value(mrb_state* mrb, double number) {
  const bool whole = std::fabs(number) <= kLargestSavedInteger && std::floor(number) == number;
  return whole ? mrb_int_value(mrb, static_cast<mrb_int>(number)) : mrb_float_value(mrb, number);
}

mrb_value save_get(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value fallback = mrb_nil_value();
  mrb_get_args(mrb, "o|o", &key, &fallback);
  const auto value = save(mrb).get(to_name(mrb, key));
  if (!value) return fallback;
  if (const auto* flag = std::get_if<bool>(&*value)) return mrb_bool_value(*flag);
  if (const auto* number = std::get_if<double>(&*value)) return number_value(mrb, *number);
  return text_value(mrb, std::get<std::string>(*value));
}

double finite_number(mrb_state* mrb, mrb_value value) {
  if (!std::isfinite(mrb_float(value))) mrb_raise(mrb, E_ARGUMENT_ERROR, "Save cannot keep infinity or NaN");
  return static_cast<double>(mrb_float(value));
}

// What is kept for a Ruby value; raises for one that cannot be kept.
PropertyValue stored_value(mrb_state* mrb, mrb_value value) {
  const bool flag = mrb_true_p(value);
  if (flag || mrb_false_p(value)) return flag;
  if (mrb_integer_p(value)) return static_cast<double>(mrb_integer(value));
  if (mrb_float_p(value)) return finite_number(mrb, value);
  if (is_name(value)) return to_name(mrb, value);
  mrb_raise(mrb, E_ARGUMENT_ERROR, "Save keeps numbers, texts and true / false");
}

mrb_value save_set(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "oo", &key, &value);
  const PropertyValue stored = stored_value(mrb, value);
  if (!save(mrb).set(to_name(mrb, key), stored)) log::warning("could not write " + save(mrb).file().string());
  return value;
}

mrb_value save_delete(mrb_state* mrb, mrb_value) {
  save(mrb).remove(name_arg(mrb));
  return mrb_nil_value();
}

mrb_value save_clear(mrb_state* mrb, mrb_value) {
  save(mrb).clear();
  return mrb_nil_value();
}

mrb_value save_keys(mrb_state* mrb, mrb_value) {
  mrb_value names = mrb_ary_new(mrb);
  for (const std::string& key : save(mrb).keys()) {
    mrb_ary_push(mrb, names, mrb_symbol_value(mrb_intern_cstr(mrb, key.c_str())));
  }
  return names;
}

}  // namespace

void install_save_bindings(mrb_state* mrb) {
  RClass* save_module = mrb_define_module(mrb, "Save");
  mrb_define_module_function(mrb, save_module, "get", save_get, MRB_ARGS_ARG(1, 1));
  mrb_define_module_function(mrb, save_module, "[]", save_get, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, save_module, "set", save_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, save_module, "[]=", save_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, save_module, "delete", save_delete, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, save_module, "clear", save_clear, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, save_module, "keys", save_keys, MRB_ARGS_NONE());
}

}  // namespace redgem
