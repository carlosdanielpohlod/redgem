#include "script/bindings/bindings.hpp"

namespace redgem {

void install_bindings(mrb_state* mrb) {
  install_game_object_bindings(mrb);
  install_transform_bindings(mrb);
  install_scene_bindings(mrb);
  install_camera_bindings(mrb);
  install_physics_bindings(mrb);
  install_runtime_bindings(mrb);
  install_ui_bindings(mrb);
  install_audio_bindings(mrb);
  install_input_bindings(mrb);
  install_animation_bindings(mrb);  // after GameObject's: it adds to that class
  install_particles_bindings(mrb);
  install_save_bindings(mrb);
  install_network_bindings(mrb);
}

}  // namespace redgem
