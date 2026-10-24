#include "script/bindings/bindings.hpp"

#include "script/bindings/animation_sync.hpp"

namespace redgem {

namespace {

// What a game is made of: its objects and the world they are in.
void install_world_bindings(mrb_state* mrb) {
  install_vector3_bindings(mrb);  // first: the others hand Vector3s out
  install_game_object_bindings(mrb);
  install_transform_bindings(mrb);
  install_scene_bindings(mrb);
  install_scatter_bindings(mrb);
  install_camera_bindings(mrb);
  install_physics_bindings(mrb);
  install_light_bindings(mrb);  // after GameObject's: it adds to that class
}

}  // namespace

void install_bindings(mrb_state* mrb) {
  install_world_bindings(mrb);
  install_runtime_bindings(mrb);
  install_game_identity_bindings(mrb);  // after the runtime's: it adds to Game
  install_ui_bindings(mrb);
  install_audio_bindings(mrb);
  install_input_bindings(mrb);
  install_animation_bindings(mrb);  // after GameObject's: it adds to that class
  install_animation_sync_bindings(mrb);
  install_particles_bindings(mrb);
  install_save_bindings(mrb);
  install_i18n_bindings(mrb);
  install_network_bindings(mrb);
  install_http_bindings(mrb);
  install_settings_bindings(mrb);
}

}  // namespace redgem
