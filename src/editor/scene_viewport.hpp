#pragma once

#include <raylib.h>

#include <string>

#include "app/frame_painter.hpp"
#include "app/presenter.hpp"
#include "editor/gated_input_source.hpp"

namespace redgem {

class Renderer;
class Window;

// The Scene view's picture: a texture as big as the view that the scene is drawn into, where the
// view is on screen, and the camera it is seen through.
struct SceneViewport {
  RenderTexture2D target{};
  int width = 0;
  int height = 0;
  float screen_x = 0.0f;
  float screen_y = 0.0f;
  bool hovered = false;
  bool focused = false;
  Camera3D camera{};

  ~SceneViewport();
  SceneViewport() = default;
  SceneViewport(const SceneViewport&) = delete;
  SceneViewport& operator=(const SceneViewport&) = delete;

  // A texture of this size (never smaller than a few pixels), kept when it already is.
  void resize(int w, int h);
  float width_pixels() const { return static_cast<float>(width); }
  float height_pixels() const { return static_cast<float>(height); }
  // The mouse in the view's own pixels.
  Vector2 mouse() const;
  // The ray into the scene through a point of the view.
  Ray ray_at(float x, float y) const;

 private:
  bool already(int w, int h) const { return w == width && h == height && target.id != 0; }
};

// Shows the running game in the Scene view, as a window of its own would: the view is the
// game's screen, drawn with the editor's renderer into the view's picture.
class GameView final : public Presenter {
 public:
  GameView(Window& window, Renderer& renderer, SceneViewport& view, const GatedInputSource& input);

  SurfaceSize surface_size() const override { return {view_.width, view_.height}; }
  int text_width(const std::string& text, int size) const override;
  // A game that locks the pointer gets it while it has the focus; Esc (Stop) gives it back.
  void capture_cursor(bool wanted) override;
  void present(const FrameView& view) override;
  // The view is there for as long as the editor is: closing the window is the editor's business.
  bool open() const override { return true; }
  float frame_seconds() const override;

  // Whether the game loaded another scene, whose assets took the place of the edited scene's.
  bool switched_scene() const { return painter_.uploaded_generation() != kFirstSceneGeneration; }

 private:
  bool pointer_granted(bool wanted) const { return input_.enabled && wanted; }

  Window& window_;
  Renderer& renderer_;
  SceneViewport& view_;
  const GatedInputSource& input_;
  FramePainter painter_;
};

}  // namespace redgem
