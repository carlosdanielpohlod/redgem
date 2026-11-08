#pragma once
#include <mruby.h>

namespace redgem::ruby_classes {
// Own root constants only: missing names never dispatch game-defined const_missing.
RClass* root_class(mrb_state* mrb, mrb_sym name);
RClass* root_class(mrb_state* mrb, const char* name);
bool constant_name(mrb_state* mrb, mrb_sym name);
// A surviving root alias of a class, preferring its last known binding; zero when none remains.
mrb_sym root_binding(mrb_state* mrb, RClass* klass, mrb_sym preferred);
}  // namespace redgem::ruby_classes
