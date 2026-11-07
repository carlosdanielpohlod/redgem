#pragma once
#include <mruby.h>
#include <string>

namespace redgem {
// Only class declarations whose evaluation defines methods and literal constants are reloadable.
// Arbitrary executable class/file setup needs a fresh Play session.
void require_reload_shape(mrb_state* mrb, const std::string& code, const std::string& class_name,
                          const std::string& superclass);
}  // namespace redgem
