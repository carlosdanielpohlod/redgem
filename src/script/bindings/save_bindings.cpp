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

mrb_value save_get(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value fallback = mrb_nil_value();
  mrb_get_args(mrb, "o|o", &key, &fallback);
  const auto value = save(mrb).get(to_name(mrb, key));
  if (!value) return fallback;
  if (const auto* flag = std::get_if<bool>(&*value)) return mrb_bool_value(*flag);
  if (const auto* number = std::get_if<double>(&*value)) {
    // Whole numbers come back as Integers: a saved count of 3 is 3, not 3.0.
    if (std::fabs(*number) > kLargestSavedInteger || std::floor(*number) != *number) return mrb_float_value(mrb, *number);
    return mrb_int_value(mrb, static_cast<mrb_int>(*number));
  }
  const auto& text = std::get<std::string>(*value);
  return mrb_str_new(mrb, text.data(), static_cast<mrb_int>(text.size()));
}

mrb_value save_set(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "oo", &key, &value);
  PropertyValue stored;
  if (mrb_true_p(value) || mrb_false_p(value)) {
    stored = mrb_true_p(value);
  } else if (mrb_integer_p(value)) {
    stored = static_cast<double>(mrb_integer(value));
  } else if (mrb_float_p(value)) {
    if (!std::isfinite(mrb_float(value))) mrb_raise(mrb, E_ARGUMENT_ERROR, "Save cannot keep infinity or NaN");
    stored = static_cast<double>(mrb_float(value));
  } else if (mrb_string_p(value) || mrb_symbol_p(value)) {
    stored = to_name(mrb, value);
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Save keeps numbers, texts and true / false");
  }
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
