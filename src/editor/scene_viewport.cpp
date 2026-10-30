#include "editor/scene_viewport.hpp"

#include <algorithm>

#include "editor/editor_config.hpp"
#include "render/renderer.hpp"
#include "render/window.hpp"

namespace redgem {

SceneViewport::~SceneViewport() {
  if (target.id != 0) UnloadRenderTexture(target);
}

void SceneViewport::resize(int w, int h) {
  w = std::max(w, editor_config::kMinViewportSize);
  h = std::max(h, editor_config::kMinViewportSize);
  if (already(w, h)) return;
  if (target.id != 0) UnloadRenderTexture(target);
  target = LoadRenderTexture(w, h);
  width = w;
  height = h;
}

Vector2 SceneViewport::mouse() const {
  const Vector2 in_window = GetMousePosition();
  return {in_window.x - screen_x, in_window.y - screen_y};
}

Ray SceneViewport::ray_at(float x, float y) const { return GetScreenToWorldRayEx({x, y}, camera, width, height); }

GameView::GameView(Window& window, Renderer& renderer, SceneViewport& view, const GatedInputSource& input,
                   bool scene_assets_uploaded)
    : window_(window),
      renderer_(renderer),
      view_(view),
      input_(input),
      painter_(renderer),
      initial_assets_changed_(!scene_assets_uploaded) {
  // Prefab expansion may replace the edited scene's assets before the first game frame.
  if (scene_assets_uploaded) painter_.assume_uploaded(kFirstSceneGeneration);
}

int GameView::text_width(const std::string& text, int size) const { return renderer_.text_width(text, size); }

int GameView::font_text_width(const std::string& text, int size, const std::string& font,
                              const std::filesystem::path& folder) const {
  return renderer_.font_text_width(text, size, font, folder);
}

void GameView::capture_cursor(bool wanted) { window_.set_cursor_locked(pointer_granted(wanted)); }

void GameView::present(const FrameView& view) {
  if (view_.target.id == 0) return;   // the view was not laid out yet: there is nothing to draw into
  renderer_.set_show_prefabs(false);  // templates are edited in the editor, hidden in the game
  view_.camera = renderer_.scene_camera(view.scene);
  painter_.paint(view, &view_.target);
}

float GameView::frame_seconds() const { return window_.frame_seconds(); }

}  // namespace redgem
