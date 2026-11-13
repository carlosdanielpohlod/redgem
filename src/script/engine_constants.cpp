#include "script/engine_constants.hpp"

#include <mruby/array.h>
#include <mruby/error.h>
#include <mruby/variable.h>

#include <utility>

#include "script/ruby_classes.hpp"

namespace redgem {

namespace {

constexpr const char* kNameSeparator = ", ";
constexpr const char* kReassignedError =
    "EngineConstantError: a script cannot set the engine's constants to something else or remove them; put back: ";

using Found = std::vector<std::pair<mrb_sym, mrb_value>>;

// mrb_iv_foreach's visitor: keeps the constants among a class's variables.
int collect(mrb_state* mrb, mrb_sym name, mrb_value value, void* found) {
  if (ruby_classes::constant_name(mrb, name)) static_cast<Found*>(found)->emplace_back(name, value);
  return 0;  // (go on)
}

RObject* top_level(mrb_state* mrb) { return reinterpret_cast<RObject*>(mrb->object_class); }

// Whether the top level still has the constant `name`, as `kept`.
bool intact(mrb_state* mrb, mrb_sym name, mrb_value kept) {
  RObject* top = top_level(mrb);
  return mrb_obj_iv_defined(mrb, top, name) && mrb_obj_eq(mrb, mrb_obj_iv_get(mrb, top, name), kept);
}

struct Restoring {
  mrb_sym name;
  mrb_value value;
};

mrb_value set_constant(mrb_state* mrb, void* data) {
  const auto* restoring = static_cast<const Restoring*>(data);
  mrb_obj_iv_set(mrb, top_level(mrb), restoring->name, restoring->value);
  return mrb_nil_value();
}

// Sets the constant back; false when Ruby refused (a script froze Object, no memory left).
bool put_back(mrb_state* mrb, mrb_sym name, mrb_value value) {
  Restoring restoring{name, value};
  mrb_bool failed = FALSE;
  mrb_protect_error(mrb, set_constant, &restoring, &failed);
  return !failed;
}

}  // namespace

void EngineConstants::remember(mrb_state* mrb) {
  values_ = mrb_ary_new(mrb);
  mrb_obj_ptr(values_)->c = nullptr;  // (out of ObjectSpace's reach, as the host's other roots)
  mrb_gc_register(mrb, values_);
  Found found;  // (gathered first: nothing is allocated while the variables are walked)
  mrb_iv_foreach(mrb, mrb_obj_value(mrb->object_class), collect, &found);
  for (const auto& [name, value] : found) {
    names_.push_back(name);
    mrb_ary_push(mrb, values_, value);
  }
}

std::vector<std::string> EngineConstants::restore(mrb_state* mrb) const {
  std::vector<std::string> restored;
  for (std::size_t i = 0; i < names_.size(); ++i) {
    const mrb_value kept = mrb_ary_ref(mrb, values_, static_cast<mrb_int>(i));
    if (intact(mrb, names_[i], kept)) continue;
    if (put_back(mrb, names_[i], kept)) restored.emplace_back(mrb_sym_name(mrb, names_[i]));
  }
  return restored;
}

std::string reassigned_constants_error(const std::vector<std::string>& names) {
  std::string listed;
  for (const std::string& name : names) listed += (listed.empty() ? "" : kNameSeparator) + name;
  return kReassignedError + listed;
}

}  // namespace redgem
