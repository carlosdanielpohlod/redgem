#include "script/ruby_classes.hpp"
#include <mruby/class.h>
#include <mruby/variable.h>

namespace redgem::ruby_classes {
namespace {
struct BindingSearch {
  RClass* klass = nullptr;
  mrb_sym found = 0;
};
int find_binding(mrb_state*, mrb_sym name, mrb_value value, void* data) {
  const bool class_value = mrb_class_p(value);
  if (!class_value) return 0;
  auto& search = *static_cast<BindingSearch*>(data);
  if (mrb_class_ptr(value) != search.klass) return 0;
  search.found = name;
  return 1;
}
}  // namespace
RClass* root_class(mrb_state* mrb, mrb_sym name) {
  const auto root = mrb_obj_value(mrb->object_class);
  if (!mrb_const_defined_at(mrb, root, name)) return nullptr;
  const auto value = mrb_const_get(mrb, root, name);
  return mrb_class_p(value) ? mrb_class_ptr(value) : nullptr;
}
RClass* root_class(mrb_state* mrb, const char* name) { return root_class(mrb, mrb_intern_cstr(mrb, name)); }
bool constant_name(mrb_state* mrb, mrb_sym name) {
  const char first = mrb_sym_name(mrb, name)[0];
  return first >= 'A' && first <= 'Z';
}
mrb_sym root_binding(mrb_state* mrb, RClass* klass, mrb_sym preferred) {
  if (root_class(mrb, preferred) == klass) return preferred;
  BindingSearch search{klass, 0};
  mrb_iv_foreach(mrb, mrb_obj_value(mrb->object_class), find_binding, &search);
  return search.found;
}
}  // namespace redgem::ruby_classes
