#include "script/bindings/bindings.hpp"

namespace redgem {

void install_bindings(mrb_state* mrb) {
  install_scene_bindings(mrb);
  install_physics_bindings(mrb);
  install_runtime_bindings(mrb);
}

}  // namespace redgem
