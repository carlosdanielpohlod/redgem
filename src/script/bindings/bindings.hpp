#pragma once

#include <mruby.h>

namespace redgem {

// Defines the native classes/modules the Ruby prelude builds upon.
void install_bindings(mrb_state* mrb);

// One for each file of natives, named after what it defines.
void install_vector3_bindings(mrb_state* mrb);
void install_game_object_bindings(mrb_state* mrb);
void install_transform_bindings(mrb_state* mrb);
void install_scene_bindings(mrb_state* mrb);
void install_scatter_bindings(mrb_state* mrb);
void install_camera_bindings(mrb_state* mrb);
void install_physics_bindings(mrb_state* mrb);
void install_light_bindings(mrb_state* mrb);
void install_runtime_bindings(mrb_state* mrb);
void install_ui_bindings(mrb_state* mrb);
void install_audio_bindings(mrb_state* mrb);
void install_input_bindings(mrb_state* mrb);
void install_animation_bindings(mrb_state* mrb);
void install_particles_bindings(mrb_state* mrb);
void install_save_bindings(mrb_state* mrb);
void install_i18n_bindings(mrb_state* mrb);
void install_network_bindings(mrb_state* mrb);
void install_http_bindings(mrb_state* mrb);

}  // namespace redgem
