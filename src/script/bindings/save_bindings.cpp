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
  if (!save(mrb).set(to_name(mrb, key), stored)) log::warning("could not write " + save(mrb).where());
  return value;
}

// Save.update(key, default = nil) { |value| new_value }: the block makes the key's new value of its
// value (default when it has none), with no other room of a dedicated server reading or writing
// the save meanwhile. Returns the new value.
mrb_value save_update(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value fallback = mrb_nil_value();
  mrb_value block = mrb_nil_value();
  mrb_get_args(mrb, "o|o&", &key, &fallback, &block);
  if (mrb_nil_p(block)) mrb_raise(mrb, E_ARGUMENT_ERROR, "Save.update needs a block making the new value");
  mrb_value made = mrb_nil_value();
  const auto change = [&](const std::optional<nlohmann::json>& current) {
    made = mrb_yield(mrb, block, current ? value_from(mrb, *current, HashKeys::Strings) : fallback);
    return json_from(mrb, made, kSaveHolder);
  };
  auto& store = save(mrb);
  if (!store.update(to_name(mrb, key), change)) log::warning("could not write " + store.where());
  return made;
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

// Save.__replace(hash): the hash's values in place of everything the slot holds, at once. Returns
// whether they were written.
mrb_value save_replace(mrb_state* mrb, mrb_value) {
  const nlohmann::json values = json_from(mrb, single_arg(mrb), kSaveHolder);
  if (!values.is_object()) mrb_raise(mrb, E_ARGUMENT_ERROR, "Save keeps a whole slot as a Hash");
  SaveStore::Values replacing;
  for (auto entry = values.begin(); entry != values.end(); ++entry) replacing[entry.key()] = entry.value();
  const bool written = save(mrb).replace(replacing);
  if (!written) log::warning("could not write " + save(mrb).where());
  return mrb_bool_value(written);
}

}  // namespace

void install_save_bindings(mrb_state* mrb) {
  // (ruby/engine/save.rb builds the game's Save on these: it checks the save's version first.)
  RClass* save_module = mrb_define_module(mrb, "Save");
  mrb_define_module_function(mrb, save_module, "__get", save_get, MRB_ARGS_ARG(1, 1));
  mrb_define_module_function(mrb, save_module, "__set", save_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, save_module, "__update", save_update, MRB_ARGS_ARG(1, 1) | MRB_ARGS_BLOCK());
  mrb_define_module_function(mrb, save_module, "__delete", save_delete, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, save_module, "__clear", save_clear, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, save_module, "__keys", save_keys, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, save_module, "__replace", save_replace, MRB_ARGS_REQ(1));
  install_save_slot_bindings(mrb);
}

}  // namespace redgem
