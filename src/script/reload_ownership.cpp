#include "script/reload_ownership.hpp"
#include <mruby/class.h>
#include <mruby/proc.h>
#include <mruby/variable.h>
#include <stdexcept>
#include "script/ruby_reload.hpp"
#include "script/script_reload.hpp"

namespace redgem {
namespace {
using Methods = std::map<mrb_sym, mrb_method_t>;
using Constants = std::map<mrb_sym, mrb_value>;
int keep_method(mrb_state*, mrb_sym name, mrb_method_t method, void* data) {
  static_cast<Methods*>(data)->emplace(name, method);
  return 0;
}
int keep_constant(mrb_state* mrb, mrb_sym name, mrb_value value, void* data) {
  const char first = mrb_sym_name(mrb, name)[0];
  if (first >= 'A' && first <= 'Z') static_cast<Constants*>(data)->emplace(name, value);
  return 0;
}
Methods methods(mrb_state* mrb, RClass* klass) {
  Methods found;
  mrb_mt_foreach(mrb, klass, keep_method, &found);
  return found;
}
Constants constants(mrb_state* mrb, RClass* klass) {
  Constants found;
  mrb_iv_foreach(mrb, mrb_obj_value(klass), keep_constant, &found);
  return found;
}
bool same_method(const mrb_method_t& a, const mrb_method_t& b) {
  if (a.flags != b.flags) return false;
  return MRB_METHOD_FUNC_P(a) ? a.as.func == b.as.func : a.as.proc == b.as.proc;
}
bool same_methods(mrb_state* mrb, RClass* a, RClass* b) {
  const auto before = methods(mrb, a);
  const auto now = methods(mrb, b);
  if (before.size() != now.size()) return false;
  for (const auto& [name, method] : before) {
    const auto found = now.find(name);
    if (found == now.end() || !same_method(method, found->second)) return false;
  }
  return true;
}
bool same_constants(mrb_state* mrb, RClass* a, RClass* b) {
  const auto before = constants(mrb, a);
  const auto now = constants(mrb, b);
  if (before.size() != now.size()) return false;
  for (const auto& [name, value] : before) {
    const auto found = now.find(name);
    if (found == now.end() || !mrb_obj_eq(mrb, value, found->second)) return false;
  }
  return true;
}
bool same_tables(mrb_state* mrb, RClass* a, RClass* b) { return same_methods(mrb, a, b) && same_constants(mrb, a, b); }
int copy_method(mrb_state* mrb, mrb_sym name, mrb_method_t method, void* target) {
  mrb_define_method_raw(mrb, static_cast<RClass*>(target), name, method);
  return 0;
}
}  // namespace
RClass* ReloadOwnership::capture(RClass* klass) {
  auto* snapshot = mrb_class_new(mrb_, mrb_->object_class);
  mrb_iv_copy(mrb_, mrb_obj_value(snapshot), mrb_obj_value(klass));
  mrb_mt_foreach(mrb_, klass, copy_method, snapshot);
  mrb_gc_register(mrb_, mrb_obj_value(snapshot));
  return snapshot;
}
void ReloadOwnership::consider(RClass* klass, const std::string& file) {
  const auto found = owners_.find(klass);
  if (found == owners_.end()) {
    auto* snapshot = capture(klass);
    mrb_gc_register(mrb_, mrb_obj_value(klass));
    owners_.emplace(klass, Owner{snapshot, file, {}});
  } else if (found->second.changed_by.empty() && !same_tables(mrb_, klass, found->second.snapshot)) {
    found->second.changed_by = file;
  }
}
int ReloadOwnership::visit(mrb_state* mrb, mrb_sym, mrb_value value, void* data) {
  const bool class_value = mrb_class_p(value);
  if (!class_value) return 0;
  auto& observation = *static_cast<Observation*>(data);
  auto& self = observation.ownership;
  auto* base = mrb_class_get(mrb, kBehaviourClass);
  auto* klass = mrb_class_ptr(value);
  if (klass != base && inherits_class(klass, base)) self.consider(klass, observation.file);
  return 0;
}
void ReloadOwnership::before_eval() {
  for (auto& [klass, owner] : owners_) {
    if (owner.changed_by.empty() && !same_tables(mrb_, klass, owner.snapshot)) owner.changed_by = "runtime callbacks";
  }
}
void ReloadOwnership::observe(const std::string& file) {
  Observation observation{*this, file};
  mrb_iv_foreach(mrb_, mrb_obj_value(mrb_->object_class), visit, &observation);
}
void ReloadOwnership::require_owner(RClass* klass, const std::string& file) {
  const auto found = owners_.find(klass);
  if (found == owners_.end()) throw std::runtime_error("restart Play: class has no recorded source owner");
  const auto& owner = found->second;
  if (owner.file != file) throw std::runtime_error("restart Play: class was first defined in " + owner.file);
  if (!owner.changed_by.empty()) throw std::runtime_error("restart Play: class was reopened by " + owner.changed_by);
  if (!same_tables(mrb_, klass, owner.snapshot))
    throw std::runtime_error("restart Play: class declarations changed in a runtime callback");
}
void ReloadOwnership::refresh(RClass* klass) {
  auto& owner = owners_.at(klass);
  auto* snapshot = capture(klass);
  mrb_gc_unregister(mrb_, mrb_obj_value(owner.snapshot));
  owner.snapshot = snapshot;
}
}  // namespace redgem
