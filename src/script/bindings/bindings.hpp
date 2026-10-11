#pragma once

#include <mruby.h>

namespace redgem {

// Defines the native classes/modules the Ruby prelude builds upon.
void install_bindings(mrb_state* mrb);

void install_scene_bindings(mrb_state* mrb);
void install_physics_bindings(mrb_state* mrb);
void install_runtime_bindings(mrb_state* mrb);
void install_input_bindings(mrb_state* mrb);
void install_animation_bindings(mrb_state* mrb);
void install_particles_bindings(mrb_state* mrb);
void install_save_bindings(mrb_state* mrb);
void install_network_bindings(mrb_state* mrb);

}  // namespace redgem
