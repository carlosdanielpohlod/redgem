// Part of EditorApp: the Scene view's picture. The edited scene drawn through the editor's camera
// with the overlays, or the running game's frames, shown in the Scene window with a hint.

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <raylib.h>

#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/scene_overlays.hpp"
#include "editor/scene_viewport.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"
#include "render/render_math.hpp"
#include "render/renderer.hpp"

namespace redgem {
namespace {

using namespace editor_config;
using render_math::to_raylib;

constexpr Vector3 kWorldUp{0.0f, 1.0f, 0.0f};
constexpr float kNoMotionBlur = 0.0f;  // strength of the blur while no game runs
// Render textures are stored upside down: flip V when showing them.
constexpr ImVec2 kFlippedUvTopLeft{0.0f, 1.0f};
constexpr ImVec2 kFlippedUvBottomRight{1.0f, 0.0f};

// The Scene view's hints, in ViewportHint's order: an icon and the key of the text after it.
struct Hint {
  const char* icon;
  const char* text;
};
constexpr Hint kViewportHints[] = {
    {ICON_FA_COMPUTER_MOUSE, "editor.scene.hint.fly"},     {ICON_FA_MOUNTAIN, "editor.scene.hint.terrain"},
    {ICON_FA_MOUNTAIN, "editor.scene.hint.sculpting"},     {ICON_FA_GAMEPAD, "editor.scene.hint.game_has_keyboard"},
    {ICON_FA_GAMEPAD, "editor.scene.hint.click_the_game"},
};

}  // namespace

void EditorApp::render_viewport_texture() {
  if (viewport_->target.id == 0) return;
  Engine* runtime = session_->runtime();
  if (runtime == nullptr) {
    draw_edited_scene();
  } else if (session_->play_state() == PlayState::Paused) {
    runtime->present();  // a running game shows each of its frames itself (GameView); a paused one has none
  }
}

// The scene as it is edited, through the editor's camera, with the editor's marks over it.
void EditorApp::draw_edited_scene() {
  SceneViewport& view = *viewport_;
  aim_viewport_camera();
  renderer_->set_show_prefabs(true);  // templates are edited in the editor, hidden in the game
  renderer_->set_motion_blur(kNoMotionBlur);
  renderer_->allow_temporal_antialias(false);  // its gizmos and grid test against the scene's depth
  sync_renderer_assets();
  renderer_->draw_world(session_->scene(), view.camera, &view.target);
  std::optional<BrushMark> brush;
  if (brush_point_) brush = BrushMark{*brush_point_, terrain_tool_.radius};
  BeginTextureMode(view.target);
  BeginMode3D(view.camera);
  draw_scene_overlays(session_->scene(), session_->scene_desc(), session_->selection(), brush);
  EndMode3D();
  EndTextureMode();
}

void EditorApp::aim_viewport_camera() {
  Camera3D& camera = viewport_->camera;
  camera.position = to_raylib(camera_.position());
  camera.target = to_raylib(camera_.target());
  camera.up = kWorldUp;
  camera.fovy = camera_.fov_y_degrees();
  camera.projection = CAMERA_PERSPECTIVE;
}

// Objects and materials created in the editor, and terrain that was sculpted.
void EditorApp::sync_renderer_assets() {
  renderer_->sync(session_->scene_desc());
  for (const int mesh : session_->document().take_changed_meshes()) {
    renderer_->refresh_mesh(session_->scene_desc(), mesh);
  }
}

// The rendered scene, filling the window. Remembers where it is on screen.
void EditorApp::draw_viewport_image() {
  const ImVec2 size = ImGui::GetContentRegionAvail();
  SceneViewport& view = *viewport_;
  view.resize(static_cast<int>(size.x), static_cast<int>(size.y));
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  view.screen_x = origin.x;
  view.screen_y = origin.y;
  ImGui::Image(ImTextureRef(static_cast<ImTextureID>(view.target.texture.id)),
               ImVec2(viewport_width(), viewport_height()), kFlippedUvTopLeft, kFlippedUvBottomRight);
}

// A frame in the accent color around the running game.
void EditorApp::draw_play_border() {
  const ImVec2 origin(viewport_->screen_x, viewport_->screen_y);
  const ImVec2 end(origin.x + viewport_width(), origin.y + viewport_height());
  ImGui::GetWindowDrawList()->AddRect(origin, end, ImGui::GetColorU32(ui::accent_color()), 0.0f, 0,
                                      kPlayBorderThickness);
}

// A hint in the bottom-left corner, on a dark pill so it reads over any scene (and stays clear
// of the game's own HUD at the top).
void EditorApp::draw_viewport_hint(bool playing) {
  const SceneViewport& view = *viewport_;
  const bool terrain_selected = session_->is_terrain(session_->selection());
  const Hint& shown = kViewportHints[static_cast<int>(
      editor_logic::viewport_hint({playing, view.focused, terrain_selected, terrain_tool_.sculpting}))];
  const char* hint = ui::icon_label(shown.icon, shown.text);
  const ImVec2 hint_size = ImGui::CalcTextSize(hint);
  const editor_logic::Rect pill =
      editor_logic::hint_pill(view.screen_x, view.screen_y, viewport_height(), hint_size.x, hint_size.y);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(pill.min_x, pill.min_y), ImVec2(pill.max_x, pill.max_y),
                      ImGui::GetColorU32(ImGuiCol_WindowBg, kOverlayAlpha), kOverlayRounding);
  draw->AddText(ImVec2(pill.min_x + kOverlayPadding, pill.min_y + kOverlayPadding),
                ImGui::GetColorU32(playing ? ui::accent_color() : ui::muted_color()), hint);
}

}  // namespace redgem
