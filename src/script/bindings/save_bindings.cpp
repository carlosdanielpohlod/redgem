// Save natives: what a game keeps between runs: what a message can carry (numbers, texts,
// true/false, Vector3, arrays and hashes of those; hashes come back with String keys). Saving nil
// removes the key.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "storage/save_store.hpp"
#include "core/log.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"

namespace redgem {
namespace {

using namespace binding;

// --- Save --------------------------------------------------------------------------------------

SaveStore& save(mrb_state* mrb) { return required(mrb, context(mrb).save, "Save is only available while a game runs"); }

mrb_value save_get(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value fallback = mrb_nil_value();
  mrb_get_args(mrb, "o|o", &key, &fallback);
  const auto value = save(mrb).get(to_name(mrb, key));
  return value ? value_from(mrb, *value, HashKeys::Strings) : fallback;
}

mrb_value save_set(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "oo", &key, &value);
  const nlohmann::json stored = json_from(mrb, value, kSaveHolder);
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
