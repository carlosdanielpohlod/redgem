// Calling the behaviours: what happens to their objects, frame after frame.

#include <mruby.h>
#include <mruby/hash.h>

#include <algorithm>

#include "script/script_host.hpp"

namespace redgem {

namespace {

constexpr const char* kUpdateEntry = "__update";        // Behaviour#__update: its timers, then its update
constexpr const char* kDestroyedEntry = "__destroyed";  // GameObject's, in the prelude

}  // namespace

bool ScriptHost::runnable(const BehaviourSlot& slot) const {
  return slot.enabled && context_.scene->resolve(slot.object) != kNoObject;
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
  const std::string& object_name = context_.scene->at(slot.object.id).name;
  report_error(slot.class_name + "#" + shown + " on '" + object_name + "' (behaviour disabled)", error);
}

std::vector<std::size_t> ScriptHost::behaviours_of(ObjectId object) const {
  const auto found = behaviours_by_object_.find(object);
  return found == behaviours_by_object_.end() ? std::vector<std::size_t>{} : found->second;
}

void ScriptHost::index_behaviours() {
  behaviours_by_object_.clear();
  for (std::size_t i = 0; i < behaviours_.size(); ++i) behaviours_by_object_[behaviours_[i].object.id].push_back(i);
}

void ScriptHost::invoke_on_object(ObjectId object, const char* method, std::initializer_list<mrb_value> args) {
  for (const std::size_t i : behaviours_of(object)) {
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

void ScriptHost::update_running(const char* entry, float dt, bool fixed) {
  const mrb_value delta = mrb_float_value(vm_.state(), dt);
  for (std::size_t i = 0; i < behaviours_.size(); ++i) {
    const bool wanted = !fixed || behaviours_[i].fixed_updates;
    if (wanted && running(behaviours_[i])) invoke(i, entry, {delta});
  }
}

void ScriptHost::fixed_update(float dt) { update_running("fixed_update", dt, true); }

void ScriptHost::update(float dt) { update_running(kUpdateEntry, dt, false); }

// The GameObjects made for a contact go on the GC arena; nothing else restores it here, so without
// this every contact with a new object would grow it for as long as the game runs.
void ScriptHost::dispatch(const ContactEvent& event) {
  const int arena = mrb_gc_arena_save(vm_.state());
  notify_contact(event);
  mrb_gc_arena_restore(vm_.state(), arena);
}

void ScriptHost::notify_contact(const ContactEvent& event) {
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
  for (const std::size_t i : behaviours_of(object)) {
    if (behaviours_[i].active()) invoke(i, "on_destroy", {});
  }
}

void ScriptHost::detach(ObjectId object) {
  // Never called while the behaviours are being walked (only between the steps of a frame), so
  // their slots can go: a destroyed object leaves nothing behind for a game that keeps spawning.
  if (behaviours_by_object_.count(object) != 0) {
    behaviours_.erase(std::remove_if(behaviours_.begin(), behaviours_.end(),
                                     [object](const BehaviourSlot& slot) { return slot.object.id == object; }),
                      behaviours_.end());
    index_behaviours();  // the others moved down
    root_behaviours();   // and those that went are no longer kept
  }
  const int arena = mrb_gc_arena_save(vm_.state());
  const RubyResult removed = vm_.call(object_value(object), kDestroyedEntry);
  mrb_gc_arena_restore(vm_.state(), arena);
  if (!removed.ok) report_error("forgetting the destroyed '" + context_.scene->at(object).name + "'", removed.error);
  mrb_hash_delete_key(vm_.state(), objects_cache_, mrb_int_value(vm_.state(), object));
}

}  // namespace redgem
