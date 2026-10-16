// Calling the behaviours: what happens to their objects, frame after frame.

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>

#include "script/script_host.hpp"

namespace redgem {

namespace {

constexpr const char* kUpdateEntry = "__update";        // Behaviour#__update: its timers, then its update
constexpr const char* kDestroyedEntry = "__destroyed";  // GameObject's, in the prelude

}  // namespace

bool ScriptHost::runnable(const BehaviourSlot& slot) const {
  return slot.enabled && context_.scene->is_alive(slot.object);
}

bool ScriptHost::running(const BehaviourSlot& slot) const { return slot.started && runnable(slot); }

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
    if (behaviours_[i].object != object) continue;
    if (running(behaviours_[i])) invoke(i, method, args);
  }
}

void ScriptHost::start_pending() {
  for (std::size_t i = 0; i < behaviours_.size(); ++i) {
    if (behaviours_[i].started) continue;
    if (!runnable(behaviours_[i])) continue;
    behaviours_[i].started = true;
    invoke(i, "start", {});
  }
}

void ScriptHost::update_running(const char* entry, float dt) {
  const mrb_value delta = mrb_float_value(vm_.state(), dt);
  for (std::size_t i = 0; i < behaviours_.size(); ++i) {
    if (running(behaviours_[i])) invoke(i, entry, {delta});
  }
}

void ScriptHost::fixed_update(float dt) { update_running("fixed_update", dt); }

void ScriptHost::update(float dt) { update_running(kUpdateEntry, dt); }

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
    if (behaviours_[i].object != object) continue;
    if (behaviours_[i].active()) invoke(i, "on_destroy", {});
  }
}

void ScriptHost::detach(ObjectId object) {
  // Never called while the behaviours are being walked (only between the steps of a frame), so
  // their slots can go: a destroyed object leaves nothing behind for a game that keeps spawning.
  behaviours_.erase(std::remove_if(behaviours_.begin(), behaviours_.end(),
                                   [object](const BehaviourSlot& slot) { return slot.object == object; }),
                    behaviours_.end());
  const RubyResult removed = vm_.call(object_value(object), kDestroyedEntry);
  if (!removed.ok) report_error("forgetting the destroyed '" + context_.scene->at(object).name + "'", removed.error);
  mrb_ary_set(vm_.state(), objects_cache_, object, mrb_nil_value());
}

}  // namespace redgem
