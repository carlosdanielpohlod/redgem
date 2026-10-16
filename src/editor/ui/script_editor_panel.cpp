#include <imgui.h>
#include <imgui_stdlib.h>

#include <vector>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

constexpr const char* kCloseScriptPopup = "Unsaved script";
constexpr const char* kCodeId = "##code";
constexpr const char* kWindowIdPrefix = "###script:";  // ImGui: what follows ### is the window's identity

void save(ScriptEditor& editor) {
  std::string error;
  if (!editor.save(error)) log::error(error);
}

// Where the script stands: changed outside, broken, saved and fine, or being edited.
void draw_status(const ScriptEditor& editor) {
  if (editor.changed_on_disk()) {
    ImGui::TextColored(warning_color(), ICON_FA_TRIANGLE_EXCLAMATION
                       "  Changed outside the editor: Save keeps this, Revert loads the file");
  } else if (!editor.syntax_error().empty()) {
    ImGui::TextColored(error_color(), ICON_FA_CIRCLE_XMARK "  %s", editor.syntax_error().c_str());
  } else if (!editor.modified()) {
    ImGui::TextColored(success_color(), ICON_FA_CIRCLE_CHECK "  Syntax OK");
  } else {
    ImGui::TextColored(muted_color(), "Unsaved changes");
  }
}

// Save, Revert, Split right, Open externally, and where the script stands.
void draw_file_buttons(ScriptEditor& editor) {
  if (ImGui::SmallButton(save_label())) save(editor);
  tooltip("Ctrl+S. Play saves and reloads scripts too.");
  ImGui::SameLine();
  ImGui::BeginDisabled(!editor.modified());
  if (ImGui::SmallButton(ICON_FA_ROTATE_LEFT "  Revert")) editor.revert();
  ImGui::EndDisabled();
}

void draw_view_buttons(const ScriptEditor& editor, const std::string& window_name, ScriptTabsResult& result) {
  if (ImGui::SmallButton(ICON_FA_TABLE_COLUMNS "  Split right")) result.split_right = window_name;
  tooltip("Show this script next to the others (or drag its tab anywhere)");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE "  Open externally")) editor.open_externally();
}

// Save and Revert, Split right and Open externally, and where the script stands.
void draw_tab_bar(ScriptEditor& editor, const std::string& window_name, ScriptTabsResult& result) {
  draw_file_buttons(editor);
  ImGui::SameLine();
  draw_view_buttons(editor, window_name, result);
  ImGui::SameLine();
  draw_status(editor);
}

// Ctrl+S, typed into the code just drawn.
bool save_pressed_in_code() {
  return ImGui::IsItemActive() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false);
}

// The code itself, filling the rest of the tab. Ctrl+S in it saves.
void draw_code(ScriptEditor& editor) {
  ImGui::PushFont(code_font(), 0.0f);
  ImGui::InputTextMultiline(kCodeId, &editor.text(), ImVec2(editor_config::kFillWidth, editor_config::kFillWidth),
                            ImGuiInputTextFlags_AllowTabInput);
  ImGui::PopFont();
  if (save_pressed_in_code()) save(editor);
}

void draw_body(ScriptEditor& editor, const std::string& window_name, ScriptTabsResult& result) {
  draw_tab_bar(editor, window_name, result);
  draw_code(editor);
}

// Save / Discard / Cancel for a tab closed with unsaved changes.
void save_and_close(ScriptWorkspace& scripts, ScriptEditor& editor, const std::string& file) {
  std::string error;
  if (editor.save(error)) {
    scripts.close(file);
  } else {
    log::error(error);
  }
}

// A button was pressed for a script that is still there.
bool still_open(bool pressed, const ScriptEditor* editor) { return pressed && editor != nullptr; }

// The dialog's three buttons. True once the question is answered (or the script is gone).
bool draw_close_choices(ScriptWorkspace& scripts, const std::string& file) {
  ScriptEditor* editor = scripts.find(file);
  bool done = editor == nullptr;
  if (still_open(ImGui::Button(save_label()), editor)) {
    save_and_close(scripts, *editor, file);
    done = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(discard_label())) {
    scripts.close(file);
    done = true;
  }
  ImGui::SameLine();
  return ImGui::Button(cancel_label()) || done;
}

// Save / Discard / Cancel for a tab closed with unsaved changes.
void draw_close_dialog(ScriptWorkspace& scripts, std::string& pending_close) {
  if (pending_close.empty()) return;
  if (!ImGui::IsPopupOpen(kCloseScriptPopup)) ImGui::OpenPopup(kCloseScriptPopup);
  if (!ImGui::BeginPopupModal(kCloseScriptPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::TextColored(warning_color(), ICON_FA_TRIANGLE_EXCLAMATION);
  ImGui::SameLine();
  ImGui::Text("Save changes to %s?", pending_close.c_str());
  ImGui::Spacing();
  if (draw_close_choices(scripts, pending_close)) {
    pending_close.clear();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

// One script's window. False when its close button was pressed.
bool draw_tab(ScriptEditor& editor, unsigned int new_tab_dock, bool focus, ScriptTabsResult& result) {
  const std::string name = script_window_name(editor.file_name());
  ImGui::SetNextWindowDockID(new_tab_dock, ImGuiCond_FirstUseEver);
  if (focus) ImGui::SetNextWindowFocus();
  bool open = true;
  const ImGuiWindowFlags flags = editor.modified() ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;
  if (ImGui::Begin(name.c_str(), &open, flags)) {
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) result.focused_dock = ImGui::GetWindowDockID();
    draw_body(editor, name, result);
  }
  ImGui::End();
  return open;
}

}  // namespace

std::string script_window_name(const std::string& file_name) {
  // The part after ### is the window's identity: the title can change, its place in the layout stays.
  return std::string(ICON_FA_GEM "  ") + file_name + kWindowIdPrefix + file_name;
}

ScriptTabsResult draw_script_tabs(ScriptWorkspace& scripts, ScriptTabsState& state, unsigned int new_tab_dock) {
  ScriptTabsResult result;
  const std::optional<std::string> focus = scripts.take_focus_request();
  std::vector<std::string> closed;
  for (const auto& tab : scripts.tabs()) {
    if (draw_tab(*tab, new_tab_dock, focus == tab->file_name(), result)) continue;
    if (tab->modified()) {
      state.pending_close = tab->file_name();  // asked about first (see draw_close_dialog)
    } else {
      closed.push_back(tab->file_name());
    }
  }
  for (const std::string& file : closed) scripts.close(file);
  draw_close_dialog(scripts, state.pending_close);
  return result;
}

}  // namespace redgem::ui
