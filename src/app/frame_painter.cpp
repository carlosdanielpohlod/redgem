#include "app/frame_painter.hpp"

namespace redgem {

bool FramePainter::prepare(const SceneDesc& desc, int generation, std::size_t bytes) {
  if (generation != prepared_generation_) drop_prepared();
  prepared_generation_ = generation;
  return renderer_.stage(desc, bytes);
}

void FramePainter::drop_prepared() {
  renderer_.drop_staged();
  prepared_generation_ = kNothingUploaded;
}

void FramePainter::take_scene(const FrameView& view) {
  if (view.scene_generation != prepared_generation_) renderer_.drop_staged();
  renderer_.upload(view.scene_desc);  // (with what was readied for it)
  uploaded_generation_ = view.scene_generation;
  prepared_generation_ = kNothingUploaded;
}

void FramePainter::follow_game(const FrameView& view) {
  if (view.scene_generation != uploaded_generation_) take_scene(view);
  // (Naming the folder again would drop the pictures already loaded from it.)
  if (view.images_dir != images_dir_) {
    images_dir_ = view.images_dir;
    renderer_.set_images_dir(images_dir_);
  }
  renderer_.set_motion_blur(view.motion_blur);
}

void FramePainter::paint(const FrameView& view, const RenderTexture2D* output) {
  paint_world(view, output);
  paint_flat(view, output);
}

void FramePainter::paint_world(const FrameView& view, const RenderTexture2D* output) {
  follow_game(view);
  renderer_.draw_world(view.scene, renderer_.scene_camera(view.scene), output, &view.particles, view.water_ripples,
                       view.debug_draw);
}

// What is flat goes over the finished picture: the game's interface, then a touch screen's controls.
void FramePainter::paint_flat(const FrameView& view, const RenderTexture2D* output) {
  if (output != nullptr) BeginTextureMode(*output);
  renderer_.draw_hud(view.hud);
  renderer_.draw_controls(view.controls);
  if (output != nullptr) EndTextureMode();
}

}  // namespace redgem
