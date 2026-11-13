#include "editor/editor_app.hpp"

#include <imgui.h>

#include "core/translations.hpp"
#include "editor/ui/dpi.hpp"
#include "editor/ui/labels.hpp"

namespace redgem {
namespace {

constexpr float kFieldWidth = 180.0f;
constexpr const char* kValueFormat = "%.2f";

void preference_field(const char* key, float& value, float min, float max) {
  ImGui::SetNextItemWidth(ui::scaled(kFieldWidth));
  ImGui::SliderFloat(tr(key), &value, min, max, kValueFormat,
                     ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
}

void draw_fields(EditorPreferences& values) {
  preference_field("editor.preferences.fly", values.fly_speed, kMinFlySpeed, kMaxFlySpeed);
  preference_field("editor.preferences.translate", values.translate_snap, kMinSnapStep, kMaxSnapStep);
  preference_field("editor.preferences.rotate", values.rotate_snap, kMinSnapStep, kMaxRotateSnap);
  preference_field("editor.preferences.scale", values.scale_snap, kMinSnapStep, kMaxSnapStep);
}

}  // namespace

void EditorApp::apply_preferences(const EditorPreferences& values) {
  preferences_ = values;
  camera_.set_fly_speed(values.fly_speed);
  gizmo_.set_preferences(values);
}

void EditorApp::save_preferences() {
  const auto file = editor_preferences_file();
  preferences_error_.clear();
  if (!write_editor_preferences(file, preferences_, edited_preferences_, preferences_error_)) return;
  apply_preferences(read_editor_preferences(file));
  ImGui::CloseCurrentPopup();
}

void EditorApp::draw_preferences_dialog() {
  const char* popup = ui::window_title("editor.preferences.title", "###editor_preferences");
  if (preferences_requested_) {
    preferences_requested_ = false;
    edited_preferences_ = preferences_;
    preferences_error_.clear();
    ImGui::OpenPopup(popup);
  }
  if (!ImGui::BeginPopupModal(popup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  draw_fields(edited_preferences_);
  draw_preferences_actions();
  ImGui::EndPopup();
}

void EditorApp::draw_preferences_actions() {
  if (ImGui::Button(tr("editor.preferences.defaults"))) edited_preferences_ = {};
  ImGui::SameLine();
  if (ImGui::Button(ui::save_label())) save_preferences();
  ImGui::SameLine();
  if (ImGui::Button(ui::cancel_label())) ImGui::CloseCurrentPopup();
  if (!preferences_error_.empty()) ImGui::TextWrapped("%s", preferences_error_.c_str());
}

}  // namespace redgem
