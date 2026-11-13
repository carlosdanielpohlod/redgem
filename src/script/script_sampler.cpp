#include "script/script_sampler.hpp"

#include <mruby/class.h>
#include <mruby/proc.h>
#include <mruby/variable.h>

#include <algorithm>

namespace redgem {
namespace {

constexpr const char* kTopLevel = "main";          // code outside any method's class
constexpr const char* kAnonymous = "(anonymous)";  // a singleton of an object that is no class
constexpr const char* kInstanceSeparator = "#";
constexpr const char* kClassSeparator = ".";

std::chrono::nanoseconds steady_now() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

bool is_class_or_module(mrb_value value) { return mrb_type(value) == MRB_TT_CLASS || mrb_type(value) == MRB_TT_MODULE; }

// The class a singleton class belongs to (Player for `def self.spawn` in Player), or nothing.
const char* attached_name(mrb_state* mrb, RClass* singleton) {
  const mrb_value attached = mrb_iv_get(mrb, mrb_obj_value(singleton), mrb_intern_lit(mrb, "__attached__"));
  return is_class_or_module(attached) ? mrb_class_name(mrb, mrb_class_ptr(attached)) : nullptr;
}

// The module itself for one of its methods (mruby finds them in a stand-in class of the module).
bool is_module_stand_in(const RClass* owner) { return owner != nullptr && owner->tt == MRB_TT_ICLASS; }

RClass* named_owner(RClass* owner) { return is_module_stand_in(owner) ? owner->c : owner; }

std::string label_of(mrb_state* mrb, RClass* found_in, mrb_sym method) {
  RClass* owner = named_owner(found_in);
  const char* name = mrb_sym_name(mrb, method);
  const std::string called = name != nullptr ? name : kAnonymous;
  if (owner == nullptr) return std::string(kTopLevel) + kInstanceSeparator + called;
  if (owner->tt != MRB_TT_SCLASS) return std::string(mrb_class_name(mrb, owner)) + kInstanceSeparator + called;
  const char* attached = attached_name(mrb, owner);
  return std::string(attached != nullptr ? attached : kAnonymous) + kClassSeparator + called;
}

}  // namespace

ScriptSampler::ScriptSampler(Clock clock) : clock_(clock ? std::move(clock) : Clock(steady_now)) {}

void ScriptSampler::set_enabled(bool enabled) {
  const bool starting = enabled && !enabled_;
  if (starting) clear();
  enabled_ = enabled;
}

void ScriptSampler::clear() {
  profile_.clear();
  names_.clear();
  instructions_ = 0;
  next_sample_ = std::chrono::nanoseconds{0};
}

// Whether to read the clock: once every kInstructionsPerClockRead instructions.
bool ScriptSampler::clock_due() {
  if (++instructions_ < kInstructionsPerClockRead) return false;
  instructions_ = 0;
  return true;
}

void ScriptSampler::on_instruction(mrb_state* mrb) {
  if (!enabled_) return;
  if (!clock_due()) return;
  const std::chrono::nanoseconds now = clock_();
  if (now < next_sample_) return;
  next_sample_ = now + kScriptSampleInterval;
  sample(mrb);
}

const std::string& ScriptSampler::name_of(mrb_state* mrb, const mrb_callinfo& frame) {
  RClass* owner = mrb_vm_ci_target_class(&frame);
  const auto key = std::make_pair(static_cast<const RClass*>(owner), frame.mid);
  const auto found = names_.find(key);
  if (found != names_.end()) return found->second;
  if (names_.size() >= kMaxCachedNames) names_.clear();  // (the stack being read holds copies)
  return names_.emplace(key, label_of(mrb, owner, frame.mid)).first->second;
}

// (Names are made of Ruby strings, let go of before the sample ends.)
void ScriptSampler::sample(mrb_state* mrb) {
  stack_.clear();
  const mrb_callinfo* first = mrb->c->cibase;
  const auto depth = static_cast<std::size_t>(mrb->c->ci - first) + 1;
  first += depth - std::min(depth, kMaxSampledDepth);
  const int arena = mrb_gc_arena_save(mrb);
  for (const mrb_callinfo* frame = first; frame <= mrb->c->ci; ++frame) {
    if (frame->mid != 0) stack_.push_back(name_of(mrb, *frame));
  }
  mrb_gc_arena_restore(mrb, arena);
  profile_.record(stack_);
}

}  // namespace redgem
