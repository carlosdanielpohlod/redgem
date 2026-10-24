// Handing the key or button a capture caught to the block waiting for it (Input.capture_next).

#include <mruby.h>

#include <vector>

#include "script/bindings/binding_helpers.hpp"
#include "script/script_host.hpp"

namespace redgem {

namespace {

constexpr const char* kInputModule = "Input";
constexpr const char* kCapturedEntry = "__captured";

// What Input.__captured takes: the binding as text, and the gamepad's number (nil for a key).
std::vector<mrb_value> captured_arguments(mrb_state* mrb, const CapturedBinding& caught) {
  const mrb_value pad = caught.pad ? mrb_int_value(mrb, static_cast<mrb_int>(*caught.pad)) : mrb_nil_value();
  return {binding::text_value(mrb, binding_text(caught.binding)), pad};
}

}  // namespace

void ScriptHost::dispatch(const CapturedBinding& caught) {
  mrb_state* mrb = vm_.state();
  const int arena = mrb_gc_arena_save(mrb);
  const RubyResult called = vm_.call_with(module_named(kInputModule), kCapturedEntry,
                                          [&caught](mrb_state* vm) { return captured_arguments(vm, caught); });
  mrb_gc_arena_restore(mrb, arena);
  if (!called.ok) report_error("an Input.capture_next block", called.error);
}

}  // namespace redgem
