#include "script/script_host.hpp"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
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
  mrb_state* mrb = vm_.state();
  if (object < 0) return mrb_nil_value();
  mrb_value cached = mrb_ary_entry(objects_cache_, object);
  if (!mrb_nil_p(cached)) return cached;
  RClass* klass = mrb_class_get(mrb, binding::kGameObjectClass);
  mrb_value wrapper = mrb_obj_value(mrb_obj_alloc(mrb, MRB_TT_OBJECT, klass));
  mrb_iv_set(mrb, wrapper, mrb_intern_cstr(mrb, binding::kIdVariable), mrb_int_value(mrb, object));
  mrb_iv_set(mrb, wrapper, mrb_intern_cstr(mrb, kBehavioursVariable), mrb_ary_new(mrb));
  mrb_ary_set(mrb, objects_cache_, object, wrapper);
  return wrapper;
}

void ScriptHost::reset_scene() {
  behaviours_.clear();
  vm_.release_kept();
  start_objects_cache();
}

void ScriptHost::start_objects_cache() {
  mrb_state* mrb = vm_.state();
  objects_cache_ = mrb_ary_new(mrb);
  mrb_gv_set(mrb, mrb_intern_cstr(mrb, kObjectsCacheGlobal), objects_cache_);
}

}  // namespace redgem
