#include "script/ruby_vm.hpp"
#include "script/ruby_context.hpp"
#include "script/ruby_classes.hpp"
#include "script/reload_ownership.hpp"
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/variable.h>
#include <mruby/proc.h>
#include <exception>
#include <stdexcept>
#include <utility>
#include "script/reload_shape.hpp"
#include "script/script_reload.hpp"

namespace redgem {
namespace {
struct ReloadRequest {
  RClass* klass;
  const std::string& code;
  const std::string& file;
  const ReloadHooks& hooks;
  ReloadOwnership* ownership;
};
void changed(mrb_state* mrb, RClass* klass) {
  mrb_write_barrier(mrb, reinterpret_cast<RBasic*>(klass));
  mrb_mc_clear_by_class(mrb, klass);
}
int remove_constant(mrb_state* mrb, mrb_sym name, mrb_value, void* target) {
  if (ruby_classes::constant_name(mrb, name)) mrb_const_remove(mrb, mrb_obj_value(static_cast<RClass*>(target)), name);
  return 0;
}
int any_method(mrb_state*, mrb_sym, mrb_method_t, void* found) {
  *static_cast<bool*>(found) = true;
  return 1;
}
bool plain_meta(mrb_state* mrb, RClass* klass) {
  if (klass->c == mrb->class_class) return true;
  if (klass->c->tt != MRB_TT_SCLASS) return false;
  const bool included_parent = klass->c->super && klass->c->super->tt == MRB_TT_ICLASS;
  if (included_parent) return false;
  bool methods = false;
  mrb_mt_foreach(mrb, klass->c, any_method, &methods);
  return !methods;
}
bool plain_class(mrb_state* mrb, RClass* klass) {
  if (!klass) return false;
  const bool simple_parent = klass->super && klass->super->tt == MRB_TT_CLASS;
  return simple_parent && plain_meta(mrb, klass) && !mrb_frozen_p(klass);
}
// Ownership of the old tables stays in a rooted class until commit or rollback. Restoring
// swaps pointers without allocating, including when compilation exhausts its budget/memory.
class ClassEdit {
 public:
  ClassEdit(mrb_state* mrb, RClass* klass) : mrb_(mrb), klass_(klass), before_(mrb_class_new(mrb, klass->super)) {
    mrb_iv_copy(mrb_, mrb_obj_value(before_), mrb_obj_value(klass_));
    std::swap(klass_->mt, before_->mt);
    changed(mrb_, before_);
    changed(mrb_, klass_);
  }
  ~ClassEdit() {
    if (committed_) return;
    std::swap(klass_->mt, before_->mt);
    std::swap(klass_->iv, before_->iv);
    changed(mrb_, before_);
    changed(mrb_, klass_);
  }
  ClassEdit(const ClassEdit&) = delete;
  ClassEdit& operator=(const ClassEdit&) = delete;
  void clear_constants() { mrb_iv_foreach(mrb_, mrb_obj_value(before_), remove_constant, klass_); }
  void commit() { committed_ = true; }

 private:
  mrb_state* mrb_;
  RClass* klass_;
  RClass* before_;
  bool committed_ = false;
};
mrb_value evaluate(mrb_state* mrb, const ReloadRequest& request) {
  RubyContext context(mrb);
  context->capture_errors = true;
  mrbc_filename(mrb, context.get(), request.file.c_str());
  const auto result = mrb_load_nstring_cxt(mrb, request.code.data(), request.code.size(), context.get());
  if (mrb->exc) mrb_exc_raise(mrb, mrb_obj_value(mrb->exc));
  return result;
}
bool names_class(mrb_state* mrb, RClass* klass, const std::string& name) {
  return ruby_classes::root_class(mrb, name.c_str()) == klass;
}
bool class_names_bound(mrb_state* mrb, const ReloadRequest& request, const std::string& name, const std::string& base) {
  return names_class(mrb, request.klass, name) && names_class(mrb, request.klass->super, base);
}
void check_target(mrb_state* mrb, const ReloadRequest& request) {
  if (!plain_class(mrb, request.klass))
    throw std::runtime_error("restart Play to reload frozen classes, class methods or mixed-in modules");
  if (class_reload_hooks(mrb, request.klass->c) != request.hooks)
    throw std::runtime_error("restart Play to reload classes with custom declaration hooks");
  const std::string name = mrb_class_name(mrb, request.klass);
  const std::string base = mrb_class_name(mrb, request.klass->super);
  require_reload_shape(mrb, request.code, name, base);
  if (!class_names_bound(mrb, request, name, base))
    throw std::runtime_error("restart Play to reload aliased or orphaned classes");
}
mrb_value replace(mrb_state* mrb, const ReloadRequest& request) {
  check_target(mrb, request);
  if (request.ownership) request.ownership->require_owner(request.klass, request.file);
  ClassEdit edit(mrb, request.klass);
  edit.clear_constants();
  const auto result = evaluate(mrb, request);
  if (request.ownership) request.ownership->refresh(request.klass);
  edit.commit();
  return result;
}
mrb_value run_reload(mrb_state* mrb, void* data) {
  try {
    return replace(mrb, *static_cast<ReloadRequest*>(data));
  } catch (const std::exception& error) {
    mrb_raise(mrb, E_RUNTIME_ERROR, error.what());
  }
  return mrb_nil_value();
}
}  // namespace
bool inherits_class(RClass* klass, RClass* base) {
  if (!base) return false;
  for (; klass; klass = klass->super)
    if (klass == base) return true;
  return false;
}
ReloadHooks class_reload_hooks(mrb_state* mrb, RClass* receiver) {
  ReloadHooks hooks{};
  for (std::size_t i = 0; i < hooks.size(); ++i) {
    const auto method = mrb_method_search(mrb, receiver, mrb_intern_cstr(mrb, kReloadHookNames[i]));
    if (MRB_METHOD_CFUNC_P(method)) hooks[i] = MRB_METHOD_CFUNC(method);
  }
  return hooks;
}
RubyResult RubyVm::reload_class(RClass* klass, const std::string& code, const std::string& file) {
  if (code.size() > kReloadSourceBytes) return {false, mrb_nil_value(), "live reload source exceeds 256 KiB"};
  ReloadRequest request{klass, code, file, reload_hooks_, reload_ownership_.get()};
  const int arena = mrb_gc_arena_save(mrb_);
  auto result = run_protected(run_reload, &request);
  result.value = mrb_nil_value();
  mrb_gc_arena_restore(mrb_, arena);
  return result;
}
}  // namespace redgem
