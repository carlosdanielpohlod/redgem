#include "script/script_host.hpp"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/variable.h>

#include <fstream>
#include <sstream>

#include "core/config.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "generated/ruby_prelude.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/network_bindings.hpp"
#include "script/script_naming.hpp"

namespace redgem {

namespace {

constexpr const char* kUpdateEntry = "__update";  // Behaviour#__update: its timers, then its update
constexpr const char* kNetworkModule = "Network";
constexpr const char* kNetworkEventEntry = "__event";
constexpr const char* kSceneResetEntry = "__scene_reset";

// What Ruby calls each kind of network event.
struct EventName {
  net::NetworkEvent::Kind kind;
  const char* name;
};
constexpr const char* kMessageEvent = "message";
constexpr EventName kEventNames[] = {
    {net::NetworkEvent::Kind::Connected, "connected"},
    {net::NetworkEvent::Kind::Failed, "failed"},
    {net::NetworkEvent::Kind::Disconnected, "disconnected"},
    {net::NetworkEvent::Kind::PeerJoined, "joined"},
    {net::NetworkEvent::Kind::PeerLeft, "left"},
    {net::NetworkEvent::Kind::Message, kMessageEvent},
};

const char* network_event_name(net::NetworkEvent::Kind kind) {
  for (const EventName& known : kEventNames) {
    if (known.kind == kind) return known.name;
  }
  return kMessageEvent;
}

// A network event as what Network.__event takes. This makes Ruby values, which can fail: only
// for where Ruby errors are caught.
std::vector<mrb_value> event_arguments(mrb_state* mrb, const net::NetworkEvent& event) {
  return {mrb_symbol_value(mrb_intern_cstr(mrb, network_event_name(event.kind))), mrb_int_value(mrb, event.peer),
          binding::text_value(mrb, event.name), value_from(mrb, event.data)};
}

constexpr const char* kObjectsCacheGlobal = "$__redgem_objects";
constexpr const char* kBehaviourClass = "Behaviour";
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

void ScriptHost::attach_scripts() {
  for (const ObjectId id : context_.scene->alive_ids()) attach_object(id);
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

void ScriptHost::scene_was_reset() {
  mrb_state* mrb = vm_.state();
  const RubyResult result = vm_.call(mrb_obj_value(mrb_module_get(mrb, kNetworkModule)), kSceneResetEntry);
  if (!result.ok) report_error("resetting the network's handlers", result.error);
}

void ScriptHost::dispatch(const net::NetworkEvent& event) {
  mrb_state* mrb = vm_.state();
  const int arena = mrb_gc_arena_save(mrb);
  // What another machine sent becomes Ruby values inside the protected call: should there be no
  // memory for them, that is one message lost, not the game.
  const RubyResult result = vm_.call_with(mrb_obj_value(mrb_module_get(mrb, kNetworkModule)), kNetworkEventEntry,
                                          [&event](mrb_state* vm) { return event_arguments(vm, event); });
  mrb_gc_arena_restore(mrb, arena);
  if (result.ok) return;
  // A failing handler does not take the game down, nor the connection. (The message's name is the
  // other machine's choice: it is shown with nothing in it that fakes log lines.)
  const std::string handler = std::string(network_event_name(event.kind)) + " " + printable(event.name);
  report_error("a Network handler (" + handler + ")", result.error);
}

void ScriptHost::attach_object(ObjectId id) {
  if (context_.scene->is_prefab(id)) return;  // templates stay inert until spawned
  const auto scripts = get_string(context_.scene->at(id).props, config::kPropScript);
  if (!scripts) return;
  for (const std::string& script : split_script_list(*scripts)) attach(id, script);
}

// The name comes from the scene's file: it is a script of the game's own folder or nothing.
bool ScriptHost::in_scripts_folder(const std::string& script) {
  if (safe_asset_name(script)) return true;
  report_error("loading a script", "the name '" + printable(script) + "' reaches outside the scripts folder");
  return false;
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

void ScriptHost::attach(ObjectId object, const std::string& script) {
  mrb_state* mrb = vm_.state();
  const std::string& object_name = context_.scene->at(object).name;
  if (!require_script(script)) return;
  const std::string class_name = class_name_for_script(script);
  if (!mrb_class_defined(mrb, class_name.c_str())) {
    report_error("object '" + object_name + "'", script + " does not define class " + class_name);
    return;
  }
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
    return not_created("object '" + object_name + "'", class_name + " must inherit from Behaviour");
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
    return not_created("object '" + context_.scene->at(object).name + "'", "add_component needs a Behaviour class");
  }
  return instantiate(object, mrb_class_ptr(klass), mrb_class_name(vm_.state(), mrb_class_ptr(klass)));
}

bool ScriptHost::require_class(const std::string& class_name) {
  const std::string script = script_for_class_name(class_name);
  std::error_code fs_error;
  if (!std::filesystem::exists(scripts_dir_ / script, fs_error)) return false;
  return require_script(script) && mrb_class_defined(vm_.state(), class_name.c_str());
}

void ScriptHost::run_scene_script(const std::string& scene_name) {
  mrb_state* mrb = vm_.state();
  const std::string script = scene_script_for(scene_name);
  std::error_code fs_error;
  if (!std::filesystem::exists(scripts_dir_ / script, fs_error) || !require_script(script)) return;
  const std::string class_name = class_name_for_script(script);
  if (!mrb_class_defined(mrb, class_name.c_str())) {
    report_error("scene '" + scene_name + "'", script + " does not define class " + class_name);
    return;
  }
  const int arena = mrb_gc_arena_save(mrb);
  const RubyResult ran = vm_.call(mrb_obj_value(mrb_class_get(mrb, "SceneScript")), "__run",
                                  {mrb_obj_value(mrb_class_get(mrb, class_name.c_str()))});
  mrb_gc_arena_restore(mrb, arena);
  if (!ran.ok) report_error(class_name + "#setup", ran.error);
}

bool ScriptHost::runnable(const BehaviourSlot& slot) const {
  return slot.enabled && context_.scene->is_alive(slot.object);
}

void ScriptHost::invoke(std::size_t index, const char* method, std::initializer_list<mrb_value> args) {
  // Callbacks may spawn objects, which appends behaviours: never hold a reference across the call.
  const mrb_value instance = behaviours_[index].instance;
  const int arena = mrb_gc_arena_save(vm_.state());
  const RubyResult result = vm_.call(instance, method, args);
  mrb_gc_arena_restore(vm_.state(), arena);
  if (!result.ok) disable_failed(index, method, result.error);
}

// A failing script keeps the game running; the behaviour is disabled.
void ScriptHost::disable_failed(std::size_t index, const char* method, const std::string& error) {
  BehaviourSlot& slot = behaviours_[index];
  slot.enabled = false;
  // (Timers and the behaviour's update run through one entry point: to the user it is "update".)
  const std::string shown = std::string(method) == kUpdateEntry ? "update" : method;
  const std::string& object_name = context_.scene->at(slot.object).name;
  report_error(slot.class_name + "#" + shown + " on '" + object_name + "' (behaviour disabled)", error);
}

void ScriptHost::invoke_on_object(ObjectId object, const char* method, std::initializer_list<mrb_value> args) {
  for (std::size_t i = 0; i < behaviours_.size(); ++i) {
    if (behaviours_[i].object == object && runnable(behaviours_[i]) && behaviours_[i].started) {
      invoke(i, method, args);
    }
  }
}

void ScriptHost::start_pending() {
  for (std::size_t i = 0; i < behaviours_.size(); ++i) {
    if (behaviours_[i].started || !runnable(behaviours_[i])) continue;
    behaviours_[i].started = true;
    invoke(i, "start", {});
  }
}

void ScriptHost::fixed_update(float dt) {
  const mrb_value delta = mrb_float_value(vm_.state(), dt);
  for (std::size_t i = 0; i < behaviours_.size(); ++i) {
    if (runnable(behaviours_[i]) && behaviours_[i].started) invoke(i, "fixed_update", {delta});
  }
}

void ScriptHost::update(float dt) {
  const mrb_value delta = mrb_float_value(vm_.state(), dt);
  for (std::size_t i = 0; i < behaviours_.size(); ++i) {
    if (runnable(behaviours_[i]) && behaviours_[i].started) invoke(i, kUpdateEntry, {delta});
  }
}

void ScriptHost::dispatch(const ContactEvent& event) {
  const mrb_value self = object_value(event.self);
  const mrb_value other = object_value(event.other);
  if (event.trigger) {
    const char* method = event.phase == ContactPhase::Enter ? "on_trigger_enter" : "on_trigger_exit";
    // Both the trigger and the object that entered it are notified.
    invoke_on_object(event.self, method, {other});
    invoke_on_object(event.other, method, {self});
  } else {
    const char* method = event.phase == ContactPhase::Enter ? "on_collision_enter" : "on_collision_exit";
    invoke_on_object(event.self, method, {other});
  }
}

void ScriptHost::on_destroyed(ObjectId object) {
  for (std::size_t i = 0; i < behaviours_.size(); ++i) {
    if (behaviours_[i].object == object && behaviours_[i].enabled && behaviours_[i].started) {
      invoke(i, "on_destroy", {});
    }
  }
}

void ScriptHost::detach(ObjectId object) {
  for (BehaviourSlot& slot : behaviours_) {
    if (slot.object == object) slot.enabled = false;
  }
  vm_.call(object_value(object), "__remove_behaviours");
}

}  // namespace redgem
