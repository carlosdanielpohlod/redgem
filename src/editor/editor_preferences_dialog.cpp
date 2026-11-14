#include "editor/editor_app.hpp"

#include <algorithm>

#include <imgui.h>

#include "core/translations.hpp"
#include "editor/ui/dpi.hpp"
#include "editor/ui/interface.hpp"
#include "editor/ui/labels.hpp"

namespace redgem {
namespace {

constexpr float kFieldWidth = 180.0f;
constexpr const char* kValueFormat = "%.2f";
constexpr float kPercent = 100.0f;

void preference_field(const char* key, float& value, float min, float max) {
  ImGui::SetNextItemWidth(ui::scaled(kFieldWidth));
  ImGui::SliderFloat(tr(key), &value, min, max, kValueFormat,
                     ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
}

void draw_key_choice(FlyBindings& bindings, FlyDirection direction, Key key) {
  if (!can_bind_fly_key(key)) return;
  const bool selected = bindings.*direction == key;
  if (ImGui::Selectable(fly_key_label(key).c_str(), selected)) rebind_fly_key(bindings, direction, key);
  if (selected) ImGui::SetItemDefaultFocus();
}

void draw_fly_binding(FlyBindings& bindings, const FlyBindingField& field) {
  ImGui::SetNextItemWidth(ui::scaled(kFieldWidth));
  const auto label = fly_key_label(bindings.*field.direction);
  if (!ImGui::BeginCombo(tr(field.label), label.c_str())) return;
  for (const Key key : all_keys()) draw_key_choice(bindings, field.direction, key);
  ImGui::EndCombo();
}

void draw_fly_bindings(FlyBindings& bindings) {
  ImGui::SeparatorText(tr("editor.preferences.fly_keys"));
  for (const auto& field : kFlyBindingFields) draw_fly_binding(bindings, field);
}

void draw_fields(EditorPreferences& values) {
  preference_field("editor.preferences.fly", values.fly_speed, kMinFlySpeed, kMaxFlySpeed);
  preference_field("editor.preferences.translate", values.translate_snap, kMinSnapStep, kMaxSnapStep);
  preference_field("editor.preferences.rotate", values.rotate_snap, kMinSnapStep, kMaxRotateSnap);
  preference_field("editor.preferences.scale", values.scale_snap, kMinSnapStep, kMaxSnapStep);
}

void draw_interface_scale(std::optional<float>& scale) {
  bool automatic = !scale.has_value();
  const bool toggled = ImGui::Checkbox(tr("editor.preferences.auto_interface_scale"), &automatic);
  if (automatic) {
    scale.reset();
    return;
  }
  if (toggled) scale = std::clamp(ui::scaled(1.0f), kMinEditorScale, kMaxEditorScale);
  float percent = *scale * kPercent;
  ImGui::SetNextItemWidth(ui::scaled(kFieldWidth));
  const bool changed =
      ImGui::SliderFloat(tr("editor.preferences.interface_scale"), &percent, kMinEditorScale * kPercent,
                         kMaxEditorScale * kPercent, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
  if (changed) scale = percent / kPercent;
}

}  // namespace

void EditorApp::apply_preferences(const EditorPreferences& values) {
  preferences_ = values;
  camera_.set_fly_speed(values.fly_speed);
  gizmo_.set_preferences(values);
  interface_->set_scale_override(values.interface_scale);
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
  draw_interface_scale(edited_preferences_.interface_scale);
  draw_fly_bindings(edited_preferences_.fly_bindings);
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
