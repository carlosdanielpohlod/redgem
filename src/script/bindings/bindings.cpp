#include "script/bindings/bindings.hpp"

namespace redgem {

void install_bindings(mrb_state* mrb) {
  install_scene_bindings(mrb);
  install_physics_bindings(mrb);
  install_runtime_bindings(mrb);
  install_input_bindings(mrb);
  install_animation_bindings(mrb);  // after the scene's: it adds to GameObject
  install_particles_bindings(mrb);
  install_save_bindings(mrb);
  install_network_bindings(mrb);
}

}  // namespace redgem
