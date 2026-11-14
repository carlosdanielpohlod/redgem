// Handing the moments a character's clips went past to the blocks waiting for them
// (Animator#on), at the start of the scripts' update.

#include <mruby.h>

#include <vector>

#include "script/bindings/binding_helpers.hpp"
#include "script/script_host.hpp"

namespace redgem {

namespace {

constexpr const char* kAnimationEventEntry = "__animation_event";  // GameObject's, in the prelude

// What GameObject#__animation_event takes: the listener, the clip, the marker's name (nil for a
// moment the script named) and where in the clip it is (0 to 1).
std::vector<mrb_value> event_arguments(mrb_state* mrb, const AnimationEvent& event) {
  const mrb_value name = event.name.empty() ? mrb_nil_value() : binding::text_value(mrb, event.name);
  return {mrb_int_value(mrb, event.listener), binding::text_value(mrb, event.clip), name,
          mrb_float_value(mrb, event.at)};
}

}  // namespace

void ScriptHost::dispatch(const AnimationEvent& event) {
  const ObjectId object = context_.scene->resolve(event.object);
  if (object == kNoObject) return;
  mrb_state* mrb = vm_.state();
  const int arena = mrb_gc_arena_save(mrb);
  const RubyResult called = vm_.call_with(object_value(object), kAnimationEventEntry,
                                          [&event](mrb_state* vm) { return event_arguments(vm, event); });
  mrb_gc_arena_restore(mrb, arena);
  if (!called.ok) report_error("an animation event block", called.error);
}

}  // namespace redgem
