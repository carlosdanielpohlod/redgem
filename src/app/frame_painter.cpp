#include "app/frame_painter.hpp"

#include "app/scene_water.hpp"
#include "render/render_math.hpp"

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
  paint_world(view, output);
  paint_flat(view, output);
}

void FramePainter::paint_world(const FrameView& view, const RenderTexture2D* output) {
  follow_game(view);
  const Camera3D camera = renderer_.scene_camera(view.scene);
  renderer_.set_underwater(underwater_of(view, camera));
  renderer_.draw_world(view.scene, camera, output, &view.particles, view.water_ripples, view.debug_draw);
}

UnderwaterView FramePainter::underwater_of(const FrameView& view, const Camera3D& camera) {
  if (view.water_surfaces == nullptr) return {};
  return underwater_view(*view.water_surfaces, render_math::to_vec3(camera.position));
}

// What is flat goes over the finished picture: the game's interface, then a touch screen's controls.
void FramePainter::paint_flat(const FrameView& view, const RenderTexture2D* output) {
  if (output != nullptr) BeginTextureMode(*output);
  renderer_.draw_hud(view.hud);
  renderer_.draw_controls(view.controls);
  if (output != nullptr) EndTextureMode();
}

}  // namespace redgem
