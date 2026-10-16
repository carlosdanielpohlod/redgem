#include "script/script_host.hpp"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/hash.h>
#include <mruby/variable.h>

#include <fstream>
#include <sstream>

#include "core/log.hpp"
#include "generated/ruby_prelude.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {

namespace {

constexpr const char* kObjectsCacheGlobal = "$__redgem_objects";
constexpr const char* kBehavioursVariable = "@behaviours";  // a GameObject's behaviours, kept by the prelude

}  // namespace

ScriptHost& script_host(mrb_state* mrb) { return *static_cast<ScriptHost*>(mrb->ud); }

ScriptHost::ScriptHost(ScriptContext& context, std::filesystem::path scripts_dir)
    : context_(context), scripts_dir_(std::move(scripts_dir)) {
  mrb_state* mrb = vm_.state();
  mrb->ud = this;
  start_objects_cache();
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

// (Each Ruby GameObject is made once for its object and kept in the cache, which holds its
// behaviours too: the cache is what keeps them alive until the object is destroyed.)
mrb_value ScriptHost::new_wrapper(const GameObject& object) {
  mrb_state* mrb = vm_.state();
  RClass* klass = mrb_class_get(mrb, binding::kGameObjectClass);
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
  return mrb_integer(serial) == static_cast<mrb_int>(object.serial);
}

void ScriptHost::reset_scene() {
  behaviours_.clear();
  start_objects_cache();
}

void ScriptHost::start_objects_cache() {
  mrb_state* mrb = vm_.state();
  // A Hash by id, not an Array: it holds only the objects scripts touched, whatever their ids,
  // where an Array has a largest size a big scene's ids go past.
  objects_cache_ = mrb_hash_new(mrb);
  mrb_gv_set(mrb, mrb_intern_cstr(mrb, kObjectsCacheGlobal), objects_cache_);
}

}  // namespace redgem
