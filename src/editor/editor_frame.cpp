// Part of EditorApp: one frame, and what the editor keeps up with between frames (the files of
// the game, what was dropped on the window, the running game, the window's title).

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <raylib.h>
#include <ImGuizmo.h>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/scene_overlays.hpp"
#include "editor/scene_viewport.hpp"
#include "editor/ui/interface.hpp"
#include "render/renderer.hpp"
#include "render/window.hpp"

namespace redgem {
namespace {

using namespace editor_config;

// The Project panel shows the folder of what a drop added: models first, then pictures, then sounds.
ui::ProjectFolder folder_to_show(const DropReport& report, ui::ProjectFolder shown) {
  if (!report.models.empty()) return ui::ProjectFolder::Models;
  if (!report.images.empty()) return ui::ProjectFolder::Images;
  return report.sounds.empty() ? shown : ui::ProjectFolder::Sounds;
}

}  // namespace

void EditorApp::frame() {
  ++frame_count_;
  frame_seconds_ = clock_();
  update_before_drawing();
  window_->begin_frame();
  ClearBackground(rgba(kViewportBackground));
  draw_interface();
  window_->end_frame();
}

void EditorApp::update_before_drawing() {
  check_files_on_disk();
  if (frame_hook_) frame_hook_(*this);
  take_dropped_files();
  handle_shortcuts();
  update_camera();
  feed_game();
  session_->tick(frame_seconds());
  if (session_->runtime() == nullptr) retire_game_view();  // the game quit by itself
  render_viewport_texture();
  update_title();
}

void EditorApp::draw_interface() {
  interface_->begin_frame();
  ImGuizmo::BeginFrame();
  gizmo_drawn_ = false;
  draw_menu_bar();
  draw_toolbar();
  draw_status_bar();
  draw_dockspace();
  draw_panels();
  end_abandoned_gizmo_drag();
  draw_dialogs();
  interface_->end_frame();
}

void EditorApp::end_abandoned_gizmo_drag() {
  if (!gizmo_drag_abandoned()) return;
  gizmo_dragging_ = false;
  session_->document().drop_checkpoint_if_unchanged();
}

void EditorApp::take_dropped_files() {
  if (!IsFileDropped()) return;
  const FilePathList dropped = LoadDroppedFiles();
  const std::vector<std::filesystem::path> files(dropped.paths, dropped.paths + dropped.count);
  UnloadDroppedFiles(dropped);
  add_files(files);
}

void EditorApp::add_files(const std::vector<std::filesystem::path>& files) {
  report_drop(add_dropped_files(session_->game_dir(), files));
  rescan_files();
}

void EditorApp::report_drop(const DropReport& report) {
  for (const std::string& model : report.models) log::info("added " + model + ": drag it from Models into the scene");
  for (const std::string& sound : report.sounds) log::info("added " + sound + " to the game's sounds");
  for (const std::string& image : report.images) {
    log::info("added " + image + " to the game's images: pick it in a material's texture slots");
  }
  for (const std::string& refused : report.refused) log::warning("not added: " + refused);
  panel_state_.project.folder = folder_to_show(report, panel_state_.project.folder);
}

void EditorApp::check_files_on_disk() {
  if (reloaded_from_disk()) renderer_->upload(session_->scene_desc());
  if (time_to(kAssetScanFrames)) {
    rescan_files();
    scripts_->sync_with_disk();
  }
}

bool EditorApp::reloaded_from_disk() { return time_to(kReloadCheckFrames) && session_->reload_if_changed_on_disk(); }

// The running game reads its keys and mouse through the Scene view, while the view has the focus.
void EditorApp::feed_game() {
  game_input_.enabled = game_has_input();
  game_input_.origin_x = viewport_->screen_x;
  game_input_.origin_y = viewport_->screen_y;
  // Nobody playing asks for the pointer: it goes back to the person at the machine.
  if (!game_input_.enabled) window_->set_cursor_locked(false);
}

bool EditorApp::game_has_input() const { return viewport_->focused && session_->play_state() == PlayState::Playing; }

void EditorApp::retire_game_view() {
  if (!game_view_) return;
  if (game_view_->switched_scene()) renderer_->upload(session_->scene_desc());  // back to the edited scene's assets
  game_view_.reset();
}

void EditorApp::update_title() {
  const std::string title = editor_logic::window_title(session_->game_dir().filename().string(), session_->scene_name(),
                                                       session_->document().dirty(), !editing());
  if (title == title_) return;
  title_ = title;
  window_->set_title(title_);
}

}  // namespace redgem
