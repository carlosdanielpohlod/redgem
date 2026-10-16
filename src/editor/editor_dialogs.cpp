// Part of EditorApp: the dialogs. Unsaved changes on leaving, the scene changed on disk, and the
// list of keyboard shortcuts.

#include "editor/editor_app.hpp"

#include <imgui.h>

#include <string>

#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"
#include "render/renderer.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr int kShortcutColumns = 2;  // keys, and what they do
constexpr const char* kShortcutsTableId = "shortcuts";
// The dialogs, by the keys of their titles (a title is its dialog's ImGui id too).
constexpr const char* kUnsavedPopup = "editor.unsaved.title";
constexpr const char* kExternalChangePopup = "editor.disk_change.title";
constexpr const char* kShortcutsPopup = "editor.shortcuts.title";

// The keys of the texts of the shortcuts list.
constexpr const char* kShortcuts[][kShortcutColumns] = {
    {"editor.keys.play", "editor.shortcuts.play"},
    {"editor.keys.stop", "editor.shortcuts.stop"},
    {kKeysSave, "editor.shortcuts.save"},
    {"editor.keys.undo_redo", "editor.shortcuts.undo_redo"},
    {kKeysDuplicate, "editor.command.duplicate"},
    {kKeysDelete, "editor.command.delete"},
    {"editor.keys.gizmo_modes", "editor.shortcuts.gizmo_modes"},
    {"editor.keys.snap", "editor.shortcuts.snap"},
    {kKeysFrame, "editor.shortcuts.frame"},
    {"editor.keys.fly", "editor.shortcuts.fly"},
    {"editor.keys.zoom", "editor.shortcuts.zoom"},
    {"editor.keys.side_panels", "editor.shortcuts.side_panels"},
    {kKeysBottomPanel, "editor.shortcuts.bottom_panel"},
};

void draw_shortcuts_table() {
  if (!ImGui::BeginTable(kShortcutsTableId, kShortcutColumns, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    return;
  for (const auto& [keys, action] : kShortcuts) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(ui::accent_color(), "%s", tr(keys));
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(tr(action));
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
  if (unsaved_dialog_wanted()) ImGui::OpenPopup(tr(kUnsavedPopup));
  if (!ImGui::BeginPopupModal(tr(kUnsavedPopup), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::TextColored(ui::warning_color(), ICON_FA_TRIANGLE_EXCLAMATION);
  ImGui::SameLine();
  ImGui::TextUnformatted(tr("editor.unsaved.question", {{"game", session_->game_dir().filename().string()}}).c_str());
  ImGui::Spacing();
  draw_unsaved_choices();
  ImGui::EndPopup();
}

bool EditorApp::unsaved_dialog_wanted() const { return close_pending_ && !ImGui::IsPopupOpen(tr(kUnsavedPopup)); }

void EditorApp::draw_unsaved_choices() {
  if (ImGui::Button(ui::save_label())) save_and_leave();
  ImGui::SameLine();
  if (ImGui::Button(ui::discard_label())) leave_without_saving();
  ImGui::SameLine();
  if (ImGui::Button(ui::cancel_label())) cancel_leaving();
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
  if (external_change_dialog_wanted()) ImGui::OpenPopup(tr(kExternalChangePopup));
  if (!ImGui::BeginPopupModal(tr(kExternalChangePopup), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  const std::string scene = session_->document().path().filename().string();
  ImGui::TextUnformatted(tr("editor.disk_change.question", {{"scene", scene}}).c_str());
  draw_external_change_choices();
  ImGui::EndPopup();
}

// The outside change is asked about once closing is not: one question at a time.
bool EditorApp::external_change_dialog_wanted() const {
  return session_->external_change_pending() && !close_pending_ && !ImGui::IsPopupOpen(tr(kExternalChangePopup));
}

void EditorApp::draw_external_change_choices() {
  if (ImGui::Button(tr("editor.disk_change.reload"))) {
    if (session_->reload_from_disk()) renderer_->upload(session_->scene_desc());
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("editor.disk_change.keep"))) {
    session_->keep_my_edits();
    ImGui::CloseCurrentPopup();
  }
}

void EditorApp::draw_shortcuts_dialog() {
  if (shortcuts_open_) {
    ImGui::OpenPopup(tr(kShortcutsPopup));
    shortcuts_open_ = false;
  }
  if (!ImGui::BeginPopupModal(tr(kShortcutsPopup), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  draw_shortcuts_table();
  if (ImGui::Button(tr("editor.shortcuts.close"))) ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

}  // namespace redgem
