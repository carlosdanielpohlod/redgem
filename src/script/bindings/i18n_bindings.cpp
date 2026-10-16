// I18n natives: the texts a game shows its players, in their language (see ruby/engine/i18n.rb).

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include "core/translations.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/game_locales.hpp"
#include "script/script_host.hpp"

namespace redgem {
namespace {

using namespace binding;

// The text of `key` in the scripts' language: the game's own (in that language, else in its
// English), else the engine's (the labels of its menus, which a game's files may change), else the
// key itself, so that a missing text shows.
std::string text_of(const ScriptHost& host, const std::string& key) {
  if (const std::string* text = host.locales().texts.find(host.locale(), key)) return *text;
  if (const std::string* text = translations::all().find(host.locale(), key)) return *text;
  return key;
}

// What a value shows as in a text: its to_s, whatever bytes that holds.
std::string shown(mrb_state* mrb, mrb_value value) {
  const mrb_value text = mrb_obj_as_string(mrb, value);
  return {RSTRING_PTR(text), static_cast<std::size_t>(RSTRING_LEN(text))};
}

// A Hash ({count: 3}) as the values of a text's placeholders ({count}).
TextValues values_of(mrb_state* mrb, mrb_value hash) {
  TextValues values;
  const mrb_value names = mrb_hash_keys(mrb, hash);
  for (mrb_int i = 0; i < RARRAY_LEN(names); ++i) {
    const mrb_value name = mrb_ary_entry(names, i);
    values.push_back({to_name(mrb, name), shown(mrb, mrb_hash_get(mrb, hash, name))});
  }
  return values;
}

mrb_value i18n_translate(mrb_state* mrb, mrb_value) {
  mrb_value key;
  mrb_value values;
  mrb_get_args(mrb, "oH", &key, &values);
  const std::string text = text_of(script_host(mrb), to_name(mrb, key));
  return text_value(mrb, fill_placeholders(text, values_of(mrb, values)));
}

mrb_value i18n_locale(mrb_state* mrb, mrb_value) { return text_value(mrb, script_host(mrb).locale()); }

mrb_value i18n_set_locale(mrb_state* mrb, mrb_value) {
  const std::string language = name_arg(mrb);
  if (!is_language_name(language)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "I18n.locale must be a language's name, such as :en or :pt_BR, not %v",
               mrb_inspect(mrb, text_value(mrb, language)));
  }
  script_host(mrb).set_locale(language);
  return mrb_nil_value();
}

mrb_value i18n_available_locales(mrb_state* mrb, mrb_value) {
  mrb_value languages = mrb_ary_new(mrb);
  for (const std::string& language : script_host(mrb).locales().languages) {
    mrb_ary_push(mrb, languages, text_value(mrb, language));
  }
  return languages;
}

}  // namespace

void install_i18n_bindings(mrb_state* mrb) {
  RClass* i18n_module = mrb_define_module(mrb, "I18n");
  mrb_define_module_function(mrb, i18n_module, "__translate", i18n_translate, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, i18n_module, "__locale", i18n_locale, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, i18n_module, "__set_locale", i18n_set_locale, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, i18n_module, "__available_locales", i18n_available_locales, MRB_ARGS_NONE());
}

}  // namespace redgem
