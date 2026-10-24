// Part of EditorApp: the game's scenes. Opening another one or making a new one (asking first
// about unsaved edits, see editor_dialogs), and saving the open one under a new name.

#include "editor/editor_app.hpp"

#include <imgui.h>

#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/game_scenes.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"
#include "render/renderer.hpp"

namespace redgem {
namespace {

constexpr float kSceneNameFieldWidth = 180.0f;  // pixels
constexpr const char* kSceneNameId = "##scene_name";
// The dialogs: their titles, then their ImGui ids (see ui::window_title), which no title shares.
const char* new_scene_popup() { return ui::window_title("editor.scenes.new_title", "###new_scene"); }
const char* save_as_popup() { return ui::window_title("editor.scenes.save_as_title", "###save_scene_as"); }

const char* scene_name_popup(SceneNameDialog dialog) {
  return dialog == SceneNameDialog::SaveAs ? save_as_popup() : new_scene_popup();
}

const char* scene_name_button(SceneNameDialog dialog) {
  return tr(dialog == SceneNameDialog::SaveAs ? "editor.scenes.save_as_button" : "editor.scenes.create");
}

}  // namespace

void EditorApp::open_scene(const std::string& name) {
  if (name == session_->scene_name()) return;  // already open
  if (!editing()) stop();
  if (session_->document().dirty()) {
    pending_ = PendingAction{PendingAction::Kind::OpenScene, name};
  } else {
    switch_scene(name);
  }
}

void EditorApp::switch_scene(const std::string& name) {
  std::string error;
  if (!session_->open_scene(name, error)) {
    log::error("cannot open scene " + name + ": " + error);
    return;
  }
  renderer_->upload(session_->scene_desc());  // the last scene's meshes and textures are let go
  ui::forget_scene(panel_state_);
  look_through_main_camera();
  stroke_started_ = false;
  gizmo_dragging_ = false;
}

bool EditorApp::new_scene(const std::string& name) {
  std::string error;
  if (!new_scene_file(session_->game_dir(), name, error)) {
    log::error("cannot make scene " + name + ": " + error);
    return false;
  }
  if (!editing()) stop();
  if (!session_->document().dirty()) return make_and_open_scene(name);
  pending_ = PendingAction{PendingAction::Kind::NewScene, name};  // made once the user answers
  return true;
}

bool EditorApp::make_and_open_scene(const std::string& name) {
  std::string error;
  const auto file = create_scene(session_->game_dir(), name, error);
  if (!file) {
    log::error("cannot make scene " + name + ": " + error);
    return false;
  }
  log::info("created " + file->string());
  rescan_files();
  switch_scene(name);
  return true;
}

bool EditorApp::save_scene_as(const std::string& name) {
  std::string error;
  if (!session_->save_as(name, error)) {
    log::error("save failed: " + error);
    return false;
  }
  rescan_files();
  return true;
}

void EditorApp::take_scene_requests(const ui::PanelContext& context) {
  if (context.scene_to_open) open_scene(*context.scene_to_open);
  if (context.new_scene_asked) ask_scene_name(SceneNameDialog::NewScene);
}

// --- The name of a new scene, or of the open one saved as another -------------------------------

void EditorApp::ask_scene_name(SceneNameDialog dialog) {
  scene_name_dialog_ = dialog;
  scene_name_dialog_opening_ = true;
  scene_name_field_.clear();
}

void EditorApp::draw_scene_name_dialog() {
  if (scene_name_dialog_ == SceneNameDialog::None) return;
  const char* popup = scene_name_popup(scene_name_dialog_);
  if (scene_name_dialog_opening_) ImGui::OpenPopup(popup);
  scene_name_dialog_opening_ = false;
  bool open = true;
  if (!ImGui::BeginPopupModal(popup, &open, ImGuiWindowFlags_AlwaysAutoResize)) {
    scene_name_dialog_ = SceneNameDialog::None;  // closed with its X
    return;
  }
  draw_scene_name_form();
  ImGui::EndPopup();
}

void EditorApp::draw_scene_name_form() {
  ImGui::TextColored(ui::muted_color(), "%s", tr("editor.scenes.name_help"));
  ImGui::SetNextItemWidth(kSceneNameFieldWidth);
  if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
  const char* button = scene_name_button(scene_name_dialog_);
  if (!ui::draw_new_scene_form(kSceneNameId, tr("editor.scenes.name_hint"), button, scene_name_field_)) return;
  if (!use_scene_name(scene_name_field_)) return;  // the console says why; the dialog stays
  scene_name_dialog_ = SceneNameDialog::None;
  ImGui::CloseCurrentPopup();
}

bool EditorApp::use_scene_name(const std::string& name) {
  return scene_name_dialog_ == SceneNameDialog::SaveAs ? save_scene_as(name) : new_scene(name);
}

}  // namespace redgem
