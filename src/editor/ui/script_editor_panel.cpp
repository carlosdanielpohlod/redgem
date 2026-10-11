#include <imgui.h>
#include <imgui_stdlib.h>

#include <vector>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

// State kept between frames. Reset when another game is opened (see reset_script_tabs_state).
std::string g_pending_close;  // a tab closed with unsaved changes, waiting for the dialog

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
    ImGui::TextColored(warning_color(), ICON_FA_TRIANGLE_EXCLAMATION "  Changed outside the editor: Save keeps this, Revert loads the file");
  } else if (!editor.syntax_error().empty()) {
    ImGui::TextColored(error_color(), ICON_FA_CIRCLE_XMARK "  %s", editor.syntax_error().c_str());
  } else if (!editor.modified()) {
    ImGui::TextColored(success_color(), ICON_FA_CIRCLE_CHECK "  Syntax OK");
  } else {
    ImGui::TextColored(muted_color(), "Unsaved changes");
  }
}

void draw_body(ScriptEditor& editor, const std::string& window_name, ScriptTabsResult& result) {
  if (ImGui::SmallButton(kSaveLabel)) save(editor);
  tooltip("Ctrl+S. Play saves and reloads scripts too.");
  ImGui::SameLine();
  ImGui::BeginDisabled(!editor.modified());
  if (ImGui::SmallButton(ICON_FA_ROTATE_LEFT "  Revert")) editor.revert();
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_TABLE_COLUMNS "  Split right")) result.split_right = window_name;
  tooltip("Show this script next to the others (or drag its tab anywhere)");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE "  Open externally")) editor.open_externally();
  ImGui::SameLine();
  draw_status(editor);
  ImGui::PushFont(code_font(), 0.0f);
  ImGui::InputTextMultiline(kCodeId, &editor.text(), ImVec2(editor_config::kFillWidth, editor_config::kFillWidth),
                            ImGuiInputTextFlags_AllowTabInput);
  ImGui::PopFont();
  const ImGuiIO& io = ImGui::GetIO();
  if (ImGui::IsItemActive() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) save(editor);
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
  ScriptEditor* editor = scripts.find(pending_close);
  bool done = editor == nullptr;
  if (ImGui::Button(kSaveLabel) && editor != nullptr) {
    std::string error;
    if (editor->save(error)) {
      scripts.close(pending_close);
    } else {
      log::error(error);
    }
    done = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(kDiscardLabel)) {
    scripts.close(pending_close);
    done = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(kCancelLabel)) done = true;
  if (done) {
    pending_close.clear();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

}  // namespace

std::string script_window_name(const std::string& file_name) {
  // The part after ### is the window's identity: the title can change, its place in the layout stays.
  return std::string(ICON_FA_GEM "  ") + file_name + kWindowIdPrefix + file_name;
}

ScriptTabsResult draw_script_tabs(ScriptWorkspace& scripts, unsigned int new_tab_dock) {
  std::string& pending_close = g_pending_close;
  ScriptTabsResult result;
  const std::optional<std::string> focus = scripts.take_focus_request();
  std::vector<std::string> closed;
  for (const auto& tab : scripts.tabs()) {
    ScriptEditor& editor = *tab;
    const std::string name = script_window_name(editor.file_name());
    ImGui::SetNextWindowDockID(new_tab_dock, ImGuiCond_FirstUseEver);
    if (focus == editor.file_name()) ImGui::SetNextWindowFocus();
    bool open = true;
    const ImGuiWindowFlags flags = editor.modified() ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;
    if (ImGui::Begin(name.c_str(), &open, flags)) {
      if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) result.focused_dock = ImGui::GetWindowDockID();
      draw_body(editor, name, result);
    }
    ImGui::End();
    if (open) continue;
    if (editor.modified()) {
      pending_close = editor.file_name();
    } else {
      closed.push_back(editor.file_name());
    }
  }
  for (const std::string& file : closed) scripts.close(file);
  draw_close_dialog(scripts, pending_close);
  return result;
}

void reset_script_tabs_state() { g_pending_close.clear(); }

}  // namespace redgem::ui
