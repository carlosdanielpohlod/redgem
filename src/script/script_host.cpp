#include "script/script_host.hpp"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/hash.h>
#include <mruby/variable.h>

#include <fstream>
#include <sstream>

#include "core/config.hpp"
#include "core/log.hpp"
#include "core/language_names.hpp"
#include "core/translations.hpp"
#include "generated/ruby_prelude.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {

namespace {

constexpr const char* kBehavioursVariable = "@behaviours";  // a GameObject's behaviours, kept by the prelude

// The translations of the game whose scripts are in `scripts_dir`; none for scripts outside a
// game's scripts folder (a file of tests run with no game around it).
GameLocales locales_around(const std::filesystem::path& scripts_dir) {
  if (scripts_dir.filename() != config::kScriptsDirName) return {};
  return read_game_locales(scripts_dir.parent_path());
}

// The language a game's scripts start in: of the game's own, the one that best serves the user's
// (even if the engine has no texts in it, see core/language_names.hpp); else the one the engine
// shows its own texts in.
std::string starting_locale(const GameLocales& locales) {
  return best_language(translations::requested_language(), locales.languages).value_or(translations::language());
}

// A script can set a wrapper's serial to anything; only the number the host wrote stands for the object.
bool holds_serial(mrb_value serial, mrb_int wanted) { return mrb_integer_p(serial) && mrb_integer(serial) == wanted; }

}  // namespace

ScriptHost& script_host(mrb_state* mrb) { return *static_cast<ScriptHost*>(mrb->ud); }

ScriptHost::ScriptHost(ScriptContext& context, std::filesystem::path scripts_dir)
    : context_(context),
      scripts_dir_(std::move(scripts_dir)),
      locales_(locales_around(scripts_dir_)),
      locale_(starting_locale(locales_)) {
  mrb_state* mrb = vm_.state();
  mrb->ud = this;
  start_roots();
  install_bindings(mrb);
  load_prelude();
}

void ScriptHost::load_prelude() {
  for (const auto& source : generated::kPreludeSources) {
    const RubyResult result = vm_.eval(source.code, source.file_name);
    if (!result.ok) report_error(std::string("prelude ") + source.file_name, result.error);
  }
}

void ScriptHost::report_error(const std::string& where, const std::string& error) {
  ++error_count_;
  log::error(where + ": " + error);
}

RubyResult ScriptHost::eval(const std::string& code, const std::string& file_name) { return vm_.eval(code, file_name); }

RubyResult ScriptHost::load_file(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) {
    RubyResult result;
    result.ok = false;
    result.error = "cannot read " + path.string();
    return result;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  return vm_.eval(buffer.str(), path.string());
}

mrb_value ScriptHost::object_value(ObjectId object) {
  if (object < 0) return mrb_nil_value();
  mrb_state* mrb = vm_.state();
  const mrb_value key = mrb_int_value(mrb, object);
  mrb_value cached = mrb_hash_get(mrb, objects_cache_, key);
  if (stands_for(cached, context_.scene->at(object))) return cached;
  mrb_value wrapper = new_wrapper(context_.scene->at(object));
  mrb_hash_set(mrb, objects_cache_, key, wrapper);
  return wrapper;
}

// (Each Ruby GameObject is made once for its object and kept in the cache until the object is
// destroyed; its behaviours are kept alive by behaviour_roots_.)
mrb_value ScriptHost::new_wrapper(const GameObject& object) {
  mrb_state* mrb = vm_.state();
  RClass* klass = class_named(binding::kGameObjectClass);
  if (klass == nullptr) return mrb_nil_value();  // a script replaced the class: no object can be shown to it
  mrb_value wrapper = mrb_obj_value(mrb_obj_alloc(mrb, MRB_TT_OBJECT, klass));
  set_variable(wrapper, binding::kIdVariable, mrb_int_value(mrb, object.id));
  set_variable(wrapper, binding::kSerialVariable, mrb_int_value(mrb, static_cast<mrb_int>(object.serial)));
  // Name and tag never change, so they stay readable once the object is gone.
  set_variable(wrapper, binding::kNameVariable, binding::text_value(mrb, object.name));
  set_variable(wrapper, binding::kTagVariable, binding::text_value(mrb, object.tag));
  set_variable(wrapper, kBehavioursVariable, mrb_ary_new(mrb));
  return wrapper;
}

void ScriptHost::set_variable(mrb_value object, const char* name, mrb_value value) {
  mrb_iv_set(vm_.state(), object, mrb_intern_cstr(vm_.state(), name), value);
}

bool ScriptHost::stands_for(mrb_value wrapper, const GameObject& object) {
  if (mrb_nil_p(wrapper)) return false;
  const mrb_value serial = mrb_iv_get(vm_.state(), wrapper, mrb_intern_cstr(vm_.state(), binding::kSerialVariable));
  return holds_serial(serial, static_cast<mrb_int>(object.serial));
}

void ScriptHost::reset_scene() {
  behaviours_.clear();
  behaviours_by_object_.clear();
  mrb_hash_clear(vm_.state(), objects_cache_);
  root_behaviours();
}

void ScriptHost::start_roots() {
  mrb_state* mrb = vm_.state();
  // A Hash by id, not an Array: it holds only the objects scripts touched, whatever their ids,
  // where an Array has a largest size a big scene's ids go past.
  objects_cache_ = mrb_hash_new(mrb);
  behaviour_roots_ = mrb_ary_new(mrb);
  // Not in a global: a script could set that to nil and have the collector free what C++ uses.
  mrb_gc_register(mrb, objects_cache_);
  mrb_gc_register(mrb, behaviour_roots_);
}

void ScriptHost::root_behaviours() {
  mrb_state* mrb = vm_.state();
  mrb_ary_clear(mrb, behaviour_roots_);
  for (const BehaviourSlot& slot : behaviours_) mrb_ary_push(mrb, behaviour_roots_, slot.instance);
}

}  // namespace redgem
