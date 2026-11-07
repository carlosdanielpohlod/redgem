#include "script/reload_ownership.hpp"
#include <mruby/array.h>
#include <mruby/proc.h>
#include <mruby/variable.h>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include "script/ruby_reload.hpp"
#include "script/script_reload.hpp"

namespace redgem {
namespace {
using Methods = std::map<mrb_sym, mrb_method_t>;
using Constants = std::map<mrb_sym, mrb_value>;
constexpr std::size_t kClassRootSlots = 1;  // the class stays alive with its recorded procs and constants
constexpr const char* kPreludeOwner = "engine prelude";
std::string source_key(const std::string& file) {
  return std::filesystem::u8path(file).lexically_normal().generic_u8string();
}
RClass* bound_class(mrb_state* mrb, mrb_sym name) {
  const auto root = mrb_obj_value(mrb->object_class);
  if (!mrb_const_defined_at(mrb, root, name)) return nullptr;
  const auto value = mrb_const_get(mrb, root, name);
  return mrb_class_p(value) ? mrb_class_ptr(value) : nullptr;
}
bool constant_name(mrb_state* mrb, mrb_sym name) {
  const char first = mrb_sym_name(mrb, name)[0];
  return first >= 'A' && first <= 'Z';
}
int keep_method(mrb_state*, mrb_sym name, mrb_method_t method, void* data) {
  static_cast<Methods*>(data)->emplace(name, method);
  return 0;
}
int keep_constant(mrb_state* mrb, mrb_sym name, mrb_value value, void* data) {
  if (constant_name(mrb, name)) static_cast<Constants*>(data)->emplace(name, value);
  return 0;
}
bool same_method(const mrb_method_t& a, const mrb_method_t& b) {
  if (a.flags != b.flags) return false;
  return MRB_METHOD_FUNC_P(a) ? a.as.func == b.as.func : a.as.proc == b.as.proc;
}
template <typename Table>
struct Comparison {
  explicit Comparison(const Table& table) : expected(table) {}
  const Table& expected;
  std::size_t seen = 0;
  bool same = true;
};
int compare_method(mrb_state*, mrb_sym name, mrb_method_t method, void* data) {
  auto& comparison = *static_cast<Comparison<Methods>*>(data);
  const auto found = comparison.expected.find(name);
  ++comparison.seen;
  comparison.same = found != comparison.expected.end() && same_method(method, found->second);
  return comparison.same ? 0 : 1;
}
int compare_constant(mrb_state* mrb, mrb_sym name, mrb_value value, void* data) {
  if (!constant_name(mrb, name)) return 0;
  auto& comparison = *static_cast<Comparison<Constants>*>(data);
  const auto found = comparison.expected.find(name);
  ++comparison.seen;
  comparison.same = found != comparison.expected.end() && mrb_obj_eq(mrb, value, found->second);
  return comparison.same ? 0 : 1;
}
mrb_value root_tables(mrb_state* mrb, RClass* klass, const Methods& methods, const Constants& constants) {
  const int arena = mrb_gc_arena_save(mrb);
  const auto roots = mrb_ary_new_capa(mrb, static_cast<mrb_int>(methods.size() + constants.size() + kClassRootSlots));
  mrb_ary_push(mrb, roots, mrb_obj_value(klass));
  for (const auto& [name, method] : methods)
    if (!MRB_METHOD_FUNC_P(method) && method.as.proc)
      mrb_ary_push(mrb, roots, mrb_obj_value(const_cast<RProc*>(method.as.proc)));
  for (const auto& [name, value] : constants) mrb_ary_push(mrb, roots, value);
  mrb_gc_register(mrb, roots);
  mrb_gc_arena_restore(mrb, arena);
  return roots;
}
}  // namespace
ReloadOwnership::ReloadOwnership(mrb_state* mrb) : mrb_(mrb) { observe(kPreludeOwner); }
ReloadOwnership::~ReloadOwnership() {
  for (const auto& [klass, owner] : owners_) mrb_gc_unregister(mrb_, owner.roots);
}
void ReloadOwnership::capture(RClass* klass, Owner& owner) {
  Methods methods;
  Constants constants;
  mrb_mt_foreach(mrb_, klass, keep_method, &methods);
  mrb_iv_foreach(mrb_, mrb_obj_value(klass), keep_constant, &constants);
  const auto roots = root_tables(mrb_, klass, methods, constants);
  if (!mrb_nil_p(owner.roots)) mrb_gc_unregister(mrb_, owner.roots);
  owner.roots = roots;
  owner.methods = std::move(methods);
  owner.constants = std::move(constants);
}
bool ReloadOwnership::same_tables(RClass* klass, const Owner& owner) const {
  Comparison<Methods> methods{owner.methods};
  mrb_mt_foreach(mrb_, klass, compare_method, &methods);
  if (!methods.same || methods.seen != owner.methods.size()) return false;
  Comparison<Constants> constants{owner.constants};
  mrb_iv_foreach(mrb_, mrb_obj_value(klass), compare_constant, &constants);
  return constants.same && constants.seen == owner.constants.size();
}
void ReloadOwnership::forget_unbound() {
  for (auto next = owners_.begin(); next != owners_.end();) {
    if (bound_class(mrb_, next->second.name) == next->first) {
      ++next;
    } else {
      mrb_gc_unregister(mrb_, next->second.roots);
      next = owners_.erase(next);
    }
  }
}
void ReloadOwnership::consider(mrb_sym name, RClass* klass, const std::string& file) {
  auto [found, inserted] = owners_.try_emplace(klass);
  auto& owner = found->second;
  if (inserted) {
    owner.name = name;
    owner.file = file;
    capture(klass, owner);
  } else if (owner.changed_by.empty() && !same_tables(klass, owner)) {
    owner.changed_by = file;
  }
}
int ReloadOwnership::visit(mrb_state*, mrb_sym name, mrb_value value, void* data) {
  const bool class_value = mrb_class_p(value);
  if (!class_value) return 0;
  auto& observation = *static_cast<Observation*>(data);
  auto* klass = mrb_class_ptr(value);
  if (klass != observation.base && inherits_class(klass, observation.base))
    observation.ownership.consider(name, klass, observation.file);
  return 0;
}
void ReloadOwnership::settle_failed_eval() {
  if (failed_file_.empty()) return;
  observe("failed evaluation of " + std::exchange(failed_file_, {}));
}
void ReloadOwnership::before_eval() {
  forget_unbound();
  settle_failed_eval();
  observe("runtime callbacks");
}
void ReloadOwnership::observe(const std::string& file) {
  auto* base = bound_class(mrb_, mrb_intern_cstr(mrb_, kBehaviourClass));
  if (!base) return;
  Observation observation{*this, base, file};
  mrb_iv_foreach(mrb_, mrb_obj_value(mrb_->object_class), visit, &observation);
}
void ReloadOwnership::begin_eval(const std::string& file) {
  if (depth_ == 0) {
    before_eval();
    active_file_ = source_key(file);
  }
  ++depth_;
}
void ReloadOwnership::complete_eval() {
  if (depth_ == 1) observe(active_file_);
}
void ReloadOwnership::end_eval(bool completed) {
  --depth_;
  if (depth_ == 0 && !completed) failed_file_ = std::move(active_file_);
}
void ReloadOwnership::require_owner(RClass* klass, const std::string& file) {
  forget_unbound();
  settle_failed_eval();
  const auto found = owners_.find(klass);
  if (found == owners_.end()) throw std::runtime_error("restart Play: class has no recorded source owner");
  const auto& owner = found->second;
  if (owner.file != source_key(file))
    throw std::runtime_error("restart Play: class was first defined in " + owner.file);
  if (!owner.changed_by.empty()) throw std::runtime_error("restart Play: class was reopened by " + owner.changed_by);
  if (!same_tables(klass, owner))
    throw std::runtime_error("restart Play: class declarations changed in a runtime callback");
}
void ReloadOwnership::refresh(RClass* klass) { capture(klass, owners_.at(klass)); }
}  // namespace redgem
