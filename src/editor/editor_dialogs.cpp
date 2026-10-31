// Part of EditorApp: the dialogs. Unsaved changes before leaving or opening another scene, the
// scene changed on disk, and the list of keyboard shortcuts.

#include "editor/editor_app.hpp"

#include <imgui.h>

#include <string>

#include "core/filled_texts.hpp"
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
// The dialogs: their titles, then their ImGui ids (see ui::window_title), which no title shares.
const char* unsaved_popup() { return ui::window_title("editor.unsaved.title", "###unsaved_changes"); }
const char* external_change_popup() { return ui::window_title("editor.disk_change.title", "###scene_changed"); }
const char* shortcuts_popup() { return ui::window_title("editor.shortcuts.title", "###shortcuts"); }

// The keys of the texts of the shortcuts list.
constexpr const char* kShortcuts[][kShortcutColumns] = {
    {"editor.keys.play", "editor.shortcuts.play"},
    {"editor.keys.stop", "editor.shortcuts.stop"},
    {kKeysSave, "editor.shortcuts.save"},
    {"editor.keys.undo_redo", "editor.shortcuts.undo_redo"},
    {kKeysDuplicate, "editor.command.duplicate"},
    {kKeysDelete, "editor.command.delete"},
    {"editor.keys.cut_copy_paste", "editor.shortcuts.cut_copy_paste"},
    {kKeysPasteInto, "editor.command.paste_into"},
    {"editor.keys.select_more", "editor.shortcuts.select_more"},
    {"editor.keys.select_rows", "editor.shortcuts.select_rows"},
    {"editor.keys.select_box", "editor.shortcuts.select_box"},
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
  draw_build_wait_dialog();
  draw_scene_name_dialog();
  draw_external_change_dialog();  // one question at a time: closing is asked first
  draw_shortcuts_dialog();
}

void EditorApp::draw_unsaved_dialog() {
  if (unsaved_dialog_wanted()) ImGui::OpenPopup(unsaved_popup());
  if (!ImGui::BeginPopupModal(unsaved_popup(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::TextColored(ui::warning_color(), ICON_FA_TRIANGLE_EXCLAMATION);
  ImGui::SameLine();
  ImGui::TextUnformatted(unsaved_question());
  ImGui::Spacing();
  draw_unsaved_choices();
  ImGui::EndPopup();
}

bool EditorApp::unsaved_dialog_wanted() const { return pending_ && !ImGui::IsPopupOpen(unsaved_popup()); }

// Leaving loses the game's unsaved work; opening another scene, the open scene's edits.
const char* EditorApp::unsaved_question() const {
  static FilledTexts leaving("editor.unsaved.question", "game");
  static FilledTexts switching("editor.scenes.unsaved_question", "scene");
  if (leaving_pending()) return leaving.with(session_->game_dir().filename().string());
  return switching.with(session_->scene_name());
}

void EditorApp::draw_unsaved_choices() {
  if (ImGui::Button(ui::save_label())) save_and_go_on();
  ImGui::SameLine();
  if (ImGui::Button(ui::discard_label())) go_on_without_saving();
  ImGui::SameLine();
  if (ImGui::Button(ui::cancel_label())) cancel_pending();
}

void EditorApp::save_and_go_on() {
  save();
  if (leaving_pending()) save_scripts();
  const PendingAction action = take_pending();
  if (still_unsaved(action)) return;  // saving failed: stay
  carry_out(action);
}

void EditorApp::go_on_without_saving() {
  const PendingAction action = take_pending();
  log::warning(action.kind == PendingAction::Kind::Leave ? "closing without saving"
                                                         : "opening " + action.scene + " without saving");
  carry_out(action);
}

void EditorApp::cancel_pending() {
  take_pending();
  exit_action_ = EditorExit::Quit;
}

PendingAction EditorApp::take_pending() {
  const PendingAction action = pending_.value_or(PendingAction{});
  pending_.reset();
  ImGui::CloseCurrentPopup();
  return action;
}

bool EditorApp::still_unsaved(const PendingAction& action) const {
  return action.kind == PendingAction::Kind::Leave ? unsaved_work() : session_->document().dirty();
}

void EditorApp::carry_out(const PendingAction& action) {
  switch (action.kind) {
    case PendingAction::Kind::Leave:
      quit_confirmed_ = true;
      break;
    case PendingAction::Kind::OpenScene:
      switch_scene(action.scene);
      break;
    case PendingAction::Kind::NewScene:
      make_and_open_scene(action.scene);
      break;
  }
}

void EditorApp::draw_external_change_dialog() {
  if (external_change_dialog_wanted()) ImGui::OpenPopup(external_change_popup());
  if (!ImGui::BeginPopupModal(external_change_popup(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  static FilledTexts question("editor.disk_change.question", "scene");
  ImGui::TextUnformatted(question.with(session_->document().path().filename().string()));
  draw_external_change_choices();
  ImGui::EndPopup();
}

// The outside change is asked about once the unsaved edits are not, and not while leaving waits
// for a build (it is asked after): one question at a time.
bool EditorApp::external_change_dialog_wanted() const {
  return session_->external_change_pending() && !pending_ && !waiting_for_build() &&
         !ImGui::IsPopupOpen(external_change_popup());
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
    ImGui::OpenPopup(shortcuts_popup());
    shortcuts_open_ = false;
  }
  if (!ImGui::BeginPopupModal(shortcuts_popup(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  draw_shortcuts_table();
  if (ImGui::Button(tr("editor.shortcuts.close"))) ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

}  // namespace redgem
