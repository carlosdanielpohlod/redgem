#pragma once

#include <filesystem>

#include "app/presenter.hpp"
#include "render/renderer.hpp"

namespace redgem {

// Draws a game's frames with a renderer: the one way a FrameView becomes a picture, shared by
// every presenter that draws (the game's own window, the editor's Scene view).
class FramePainter {
 public:
  // `renderer` has to outlive the painter.
  explicit FramePainter(Renderer& renderer) : renderer_(renderer) {}

  // Draws the frame into `output`, or onto the screen when null (then between the window's
  // begin_frame and end_frame). The scene's meshes, materials and textures are uploaded first
  // when the game changed scene since the last frame painted.
  void paint(const FrameView& view, const RenderTexture2D* output = nullptr);
  // The two halves of paint, for a picture whose world is drawn at another size than what is flat
  // over it (see DisplaySettings::render_scale): the world, then the interface and touch controls.
  void paint_world(const FrameView& view, const RenderTexture2D* output);
  void paint_flat(const FrameView& view, const RenderTexture2D* output = nullptr);
  // Says the renderer already holds the assets of this generation of the game's scene (the editor
  // plays the scene it was drawing): they are not uploaded again.
  void assume_uploaded(int scene_generation) { uploaded_generation_ = scene_generation; }
  // The generation of the game's scene whose assets the renderer holds; kNothingUploaded before any.
  int uploaded_generation() const { return uploaded_generation_; }

  static constexpr int kNothingUploaded = -1;

 private:
  void follow_game(const FrameView& view);

  Renderer& renderer_;
  int uploaded_generation_ = kNothingUploaded;
  std::filesystem::path images_dir_;
};

}  // namespace redgem
