#pragma once

#include <mruby.h>

namespace redgem {

// Defines the native classes/modules the Ruby prelude builds upon.
void install_bindings(mrb_state* mrb);

void install_scene_bindings(mrb_state* mrb);
void install_physics_bindings(mrb_state* mrb);
void install_runtime_bindings(mrb_state* mrb);

}  // namespace redgem
