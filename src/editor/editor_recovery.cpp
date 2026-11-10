#include "editor/editor_app.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>

#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem {
namespace {
constexpr float kRecoveryWidth = 480;
constexpr float kWindowMargin = 32;
void recovery_dialog_size() {
  const auto available = ImGui::GetMainViewport()->WorkSize.x - kWindowMargin;
  const auto width = std::max(1.0f, std::min(kRecoveryWidth, available));
  ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, FLT_MAX));
}
const char* recovery_popup() { return ui::window_title("editor.recovery.title", "###scene_recovery"); }
}  // namespace
bool EditorApp::can_autosave_scene() const {
  return editing() && !pending_ && !waiting_for_build() && !interaction_active();
}
void EditorApp::autosave_scene() {
  if (!can_autosave_scene()) return;
  session_->autosave(frame_seconds());
}
void EditorApp::draw_recovery_dialog() {
  const bool wanted = session_->recovery_pending() && !pending_ && !waiting_for_build();
  if (!wanted) return;
  if (!ImGui::IsPopupOpen(recovery_popup())) ImGui::OpenPopup(recovery_popup());
  recovery_dialog_size();
  if (!ImGui::BeginPopupModal(recovery_popup(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::TextWrapped("%s", tr("editor.recovery.question"));
  if (session_->recovery_source_changed()) ImGui::TextWrapped("%s", tr("editor.recovery.changed"));
  draw_recovery_choices();
  ImGui::EndPopup();
}
void EditorApp::draw_recovery_choices() {
  if (ImGui::Button(tr("editor.recovery.recover"))) recover_scene();
  ImGui::SameLine();
  if (!ImGui::Button(tr("editor.recovery.discard"))) return;
  std::string error;
  if (!session_->discard_recovery(error)) {
    log::warning(error);
    session_->ignore_recovery();
  }
  ImGui::CloseCurrentPopup();
}
bool EditorApp::recover_scene() {
  std::string error;
  if (!session_->recover_scene(error)) {
    log::error("cannot recover scene: " + error);
    return false;
  }
  upload_preview_assets();
  look_through_main_camera();
  ImGui::CloseCurrentPopup();
  return true;
}
}  // namespace redgem
