// Part of EditorApp: the dialogs. Unsaved changes on leaving, the scene changed on disk, and the
// list of keyboard shortcuts.

#include "editor/editor_app.hpp"

#include <imgui.h>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/style.hpp"
#include "render/renderer.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr int kShortcutColumns = 2;  // keys, and what they do
constexpr const char* kShortcutsTableId = "shortcuts";
constexpr const char* kUnsavedPopup = "Unsaved changes";
constexpr const char* kExternalChangePopup = "Scene changed on disk";
constexpr const char* kShortcutsPopup = "Keyboard shortcuts";

constexpr const char* kShortcuts[][kShortcutColumns] = {
    {"Ctrl+P", "Play / Stop"},
    {"Esc", "Stop playing"},
    {kKeysSave, "Save scene"},
    {"Ctrl+Z / Ctrl+Y", "Undo / Redo"},
    {kKeysDuplicate, "Duplicate"},
    {kKeysDelete, "Delete"},
    {"W / E / R", "Move / Rotate / Scale"},
    {"Hold Ctrl", "Snap while dragging"},
    {kKeysFrame, "Frame selection"},
    {"Right mouse + WASD/QE", "Fly the camera"},
    {"Mouse wheel", "Zoom"},
    {"Ctrl+B / Ctrl+Alt+B", "Hide / show the left / right panel"},
    {kKeysBottomPanel, "Hide / show the bottom panel"},
};

void draw_shortcuts_table() {
  if (!ImGui::BeginTable(kShortcutsTableId, kShortcutColumns, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    return;
  for (const auto& [keys, action] : kShortcuts) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(ui::accent_color(), "%s", keys);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(action);
  }
  ImGui::EndTable();
}

}  // namespace

void EditorApp::draw_dialogs() {
  draw_unsaved_dialog();
  draw_external_change_dialog();  // one question at a time: closing is asked first
  draw_shortcuts_dialog();
}

void EditorApp::draw_unsaved_dialog() {
  if (unsaved_dialog_wanted()) ImGui::OpenPopup(kUnsavedPopup);
  if (!ImGui::BeginPopupModal(kUnsavedPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::TextColored(ui::warning_color(), ICON_FA_TRIANGLE_EXCLAMATION);
  ImGui::SameLine();
  ImGui::Text("Save changes to %s before closing?", session_->game_dir().filename().string().c_str());
  ImGui::Spacing();
  draw_unsaved_choices();
  ImGui::EndPopup();
}

bool EditorApp::unsaved_dialog_wanted() const { return close_pending_ && !ImGui::IsPopupOpen(kUnsavedPopup); }

void EditorApp::draw_unsaved_choices() {
  if (ImGui::Button(ui::kSaveLabel)) save_and_leave();
  ImGui::SameLine();
  if (ImGui::Button(ui::kDiscardLabel)) leave_without_saving();
  ImGui::SameLine();
  if (ImGui::Button(ui::kCancelLabel)) cancel_leaving();
}

void EditorApp::save_and_leave() {
  save();
  save_scripts();
  // Stay open if saving the scene or a script failed.
  quit_confirmed_ = !unsaved_work();
  close_pending_ = false;
  ImGui::CloseCurrentPopup();
}

void EditorApp::leave_without_saving() {
  log::warning("closing without saving");
  quit_confirmed_ = true;
  ImGui::CloseCurrentPopup();
}

void EditorApp::cancel_leaving() {
  close_pending_ = false;
  exit_action_ = EditorExit::Quit;
  ImGui::CloseCurrentPopup();
}

void EditorApp::draw_external_change_dialog() {
  if (external_change_dialog_wanted()) ImGui::OpenPopup(kExternalChangePopup);
  if (!ImGui::BeginPopupModal(kExternalChangePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::Text("%s changed on disk (Blender export?) while you have unsaved edits.",
              session_->document().path().filename().string().c_str());
  draw_external_change_choices();
  ImGui::EndPopup();
}

// The outside change is asked about once closing is not: one question at a time.
bool EditorApp::external_change_dialog_wanted() const {
  return session_->external_change_pending() && !close_pending_ && !ImGui::IsPopupOpen(kExternalChangePopup);
}

void EditorApp::draw_external_change_choices() {
  if (ImGui::Button("Reload from disk")) {
    if (session_->reload_from_disk()) renderer_->upload(session_->scene_desc());
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Keep my edits")) {
    session_->keep_my_edits();
    ImGui::CloseCurrentPopup();
  }
}

void EditorApp::draw_shortcuts_dialog() {
  if (shortcuts_open_) {
    ImGui::OpenPopup(kShortcutsPopup);
    shortcuts_open_ = false;
  }
  if (!ImGui::BeginPopupModal(kShortcutsPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  draw_shortcuts_table();
  if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

}  // namespace redgem
