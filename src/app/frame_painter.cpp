#include "app/frame_painter.hpp"

namespace redgem {

void FramePainter::follow_game(const FrameView& view) {
  if (view.scene_generation != uploaded_generation_) {
    renderer_.upload(view.scene_desc);
    uploaded_generation_ = view.scene_generation;
  }
  // (Naming the folder again would drop the pictures already loaded from it.)
  if (view.images_dir != images_dir_) {
    images_dir_ = view.images_dir;
    renderer_.set_images_dir(images_dir_);
  }
  renderer_.set_motion_blur(view.motion_blur);
}

void FramePainter::paint(const FrameView& view, const RenderTexture2D* output) {
  follow_game(view);
  renderer_.draw_world(view.scene, renderer_.scene_camera(view.scene), output, &view.particles);
  // What is flat goes over the finished picture: the game's interface, then a touch screen's controls.
  if (output != nullptr) BeginTextureMode(*output);
  renderer_.draw_hud(view.hud);
  renderer_.draw_controls(view.controls);
  if (output != nullptr) EndTextureMode();
}

}  // namespace redgem
