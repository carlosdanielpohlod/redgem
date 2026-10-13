// The game's scripts, and the behaviours made of the classes they define.

#include <mruby.h>
#include <mruby/class.h>

#include "core/safe_path.hpp"
#include "scene/props.hpp"
#include "script/script_host.hpp"
#include "script/script_naming.hpp"

namespace redgem {

namespace {

constexpr const char* kBehaviourClass = "Behaviour";
constexpr const char* kSceneScriptClass = "SceneScript";  // what a scene's setup script inherits from
constexpr const char* kRunSceneScriptEntry = "__run";

std::string object_called(const std::string& name) { return "object '" + name + "'"; }

}  // namespace

void ScriptHost::attach_scripts() {
  for (const ObjectId id : context_.scene->alive_ids()) attach_object(id);
}

void ScriptHost::attach_object(ObjectId id) {
  if (context_.scene->is_prefab(id)) return;  // templates stay inert until spawned
  const auto scripts = get_string(context_.scene->at(id).props, props::kScript);
  if (!scripts) return;
  for (const std::string& script : split_script_list(*scripts)) attach(id, script);
}

// The name comes from the scene's file: it is a script of the game's own folder or nothing.
bool ScriptHost::in_scripts_folder(const std::string& script) {
  if (safe_asset_name(script)) return true;
  report_error("loading a script", "the name '" + printable(script) + "' reaches outside the scripts folder");
  return false;
}

bool ScriptHost::has_script(const std::string& script) const {
  std::error_code fs_error;
  return std::filesystem::exists(scripts_dir_ / script, fs_error);
}

bool ScriptHost::require_script(const std::string& script) {
  if (loaded_scripts_.count(script) != 0) return true;
  if (!in_scripts_folder(script)) return false;
  // A script that uses its own class before defining it would otherwise load itself forever.
  if (!loading_scripts_.insert(script).second) return false;
  const std::filesystem::path path = scripts_dir_ / script;
  const RubyResult result = load_file(path);
  loading_scripts_.erase(script);
  if (!result.ok) {
    report_error("loading " + path.string(), result.error);
    return false;
  }
  loaded_scripts_.insert(script);
  return true;
}

bool ScriptHost::defines_its_class(const std::string& script, const std::string& class_name, const std::string& where) {
  if (mrb_class_defined(vm_.state(), class_name.c_str())) return true;
  report_error(where, script + " does not define class " + class_name);
  return false;
}

void ScriptHost::attach(ObjectId object, const std::string& script) {
  mrb_state* mrb = vm_.state();
  const std::string& object_name = context_.scene->at(object).name;
  if (!require_script(script)) return;
  const std::string class_name = class_name_for_script(script);
  if (!defines_its_class(script, class_name, object_called(object_name))) return;
  instantiate(object, mrb_class_get(mrb, class_name.c_str()), class_name);
}

bool ScriptHost::is_behaviour_class(RClass* klass) {
  mrb_state* mrb = vm_.state();
  if (!mrb_obj_is_kind_of(mrb, mrb_obj_value(klass), mrb->class_class)) return false;
  if (!mrb_class_defined(mrb, kBehaviourClass)) return false;
  const mrb_value behaviour = mrb_obj_value(mrb_class_get(mrb, kBehaviourClass));
  return mrb_test(vm_.call(mrb_obj_value(klass), "<=", {behaviour}).value);
}

mrb_value ScriptHost::not_created(const std::string& where, const std::string& error) {
  report_error(where, error);
  return mrb_nil_value();
}

mrb_value ScriptHost::instantiate(ObjectId object, RClass* klass, const std::string& class_name) {
  const std::string& object_name = context_.scene->at(object).name;
  if (!is_behaviour_class(klass)) {
    return not_created(object_called(object_name), class_name + " must inherit from Behaviour");
  }
  const RubyResult instance = vm_.call(mrb_obj_value(klass), "new");
  if (!instance.ok) return not_created(class_name + ".new on '" + object_name + "'", instance.error);
  const RubyResult attached = vm_.call(instance.value, "__attach", {object_value(object)});
  if (!attached.ok) return not_created(class_name + "#__attach", attached.error);
  vm_.keep_alive(instance.value);
  behaviours_.push_back({object, instance.value, class_name, false, true});
  return instance.value;
}

mrb_value ScriptHost::add_behaviour(ObjectId object, mrb_value klass) {
  const bool is_class = mrb_class_p(klass);
  if (!is_class) {
    return not_created(object_called(context_.scene->at(object).name), "add_component needs a Behaviour class");
  }
  return instantiate(object, mrb_class_ptr(klass), mrb_class_name(vm_.state(), mrb_class_ptr(klass)));
}

bool ScriptHost::require_class(const std::string& class_name) {
  const std::string script = script_for_class_name(class_name);
  const bool loaded = has_script(script) && require_script(script);
  if (!loaded) return false;
  return mrb_class_defined(vm_.state(), class_name.c_str());
}

void ScriptHost::run_scene_script(const std::string& scene_name) {
  mrb_state* mrb = vm_.state();
  const std::string script = scene_script_for(scene_name);
  const bool loaded = has_script(script) && require_script(script);
  if (!loaded) return;
  const std::string class_name = class_name_for_script(script);
  if (!defines_its_class(script, class_name, "scene '" + scene_name + "'")) return;
  const int arena = mrb_gc_arena_save(mrb);
  const RubyResult ran = vm_.call(mrb_obj_value(mrb_class_get(mrb, kSceneScriptClass)), kRunSceneScriptEntry,
                                  {mrb_obj_value(mrb_class_get(mrb, class_name.c_str()))});
  mrb_gc_arena_restore(mrb, arena);
  if (!ran.ok) report_error(class_name + "#setup", ran.error);
}

}  // namespace redgem
