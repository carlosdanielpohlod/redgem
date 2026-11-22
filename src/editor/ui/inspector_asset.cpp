// Inspector card of an asset chosen in the Project panel: its import settings (see
// core/import_settings.hpp), edited as a draft, then applied (its sidecar written, and a model
// imported again wherever the open scene holds it) or put back as the file has them.

#include <imgui.h>

#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;
using import_settings::Field;

constexpr const char* kTrueText = "true";
constexpr const char* kFalseText = "false";
constexpr const char* kIdPrefix = "##";
constexpr float kNumberSpeed = 0.01f;
constexpr const char* kFractionMarks = ".eE";

// The text key of each import setting's label, written out whole so a key no field shows is found.
constexpr std::pair<const char*, const char*> kFieldKeys[] = {
    {"scale", "editor.asset.field.scale"},
    {"up_axis", "editor.asset.field.up_axis"},
    {"colliders", "editor.asset.field.colliders"},
    {"materials", "editor.asset.field.materials"},
    {"color_space", "editor.asset.field.color_space"},
    {"normal_map", "editor.asset.field.normal_map"},
    {"max_size", "editor.asset.field.max_size"},
    {"mipmaps", "editor.asset.field.mipmaps"},
    {"compression", "editor.asset.field.compression"},
    {"stream", "editor.asset.field.stream"},
    {"loop", "editor.asset.field.loop"},
    {"volume", "editor.asset.field.volume"},
    {"mono_3d", "editor.asset.field.mono_3d"},
};

// A field's label: its text, or its own name when no text is kept for it.
std::string label_of(const Field& field) {
  for (const auto& [name, key] : kFieldKeys) {
    if (field.name == name) return tr(key);
  }
  return field.name;
}

bool is_switch(const Field& field) { return field.value == kTrueText || field.value == kFalseText; }
bool is_fraction(const Field& field) { return field.value.find_first_of(kFractionMarks) != std::string::npos; }

std::string id_of(const Field& field) { return kIdPrefix + field.name; }

// Sets one field of the draft from text; a value it cannot take is said under the fields.
void edit(AssetState& asset, const Field& field, const std::string& text) {
  std::string error;
  if (!asset.draft) return;
  if (import_settings::set(*asset.draft, field.name, text, error)) {
    asset.problem.clear();
  } else {
    asset.problem = error;
  }
}

void draw_choice(AssetState& asset, const Field& field) {
  if (!ImGui::BeginCombo(id_of(field).c_str(), field.value.c_str())) return;
  for (const std::string& choice : field.choices) {
    if (ImGui::Selectable(choice.c_str(), choice == field.value)) edit(asset, field, choice);
  }
  ImGui::EndCombo();
}

void draw_switch(AssetState& asset, const Field& field) {
  bool on = field.value == kTrueText;
  if (ImGui::Checkbox(id_of(field).c_str(), &on)) edit(asset, field, on ? kTrueText : kFalseText);
}

void draw_number(AssetState& asset, const Field& field) {
  if (is_fraction(field)) {
    float number = std::stof(field.value);
    if (ImGui::DragFloat(id_of(field).c_str(), &number, kNumberSpeed)) edit(asset, field, std::to_string(number));
    return;
  }
  int number = std::stoi(field.value);
  if (ImGui::InputInt(id_of(field).c_str(), &number)) edit(asset, field, std::to_string(number));
}

void draw_field(AssetState& asset, const Field& field) {
  property_label(label_of(field).c_str());
  ImGui::SetNextItemWidth(kFillWidth);
  if (!field.choices.empty()) return draw_choice(asset, field);
  if (is_switch(field)) return draw_switch(asset, field);
  draw_number(asset, field);
}

// Whether the file was taken away since it was chosen (said under the fields).
bool gone(AssetState& asset) {
  std::error_code fs_error;
  if (std::filesystem::is_regular_file(asset.file, fs_error)) return false;
  asset.problem = tr("editor.asset.gone", {{"file", asset.file.filename().string()}});
  return true;
}

// A draft to write, for a file still there.
bool ready_to_apply(AssetState& asset) { return asset.draft.has_value() && !gone(asset); }

void reimport(PanelContext& context, const AssetState& asset) {
  std::string error;
  const int done = context.session.reimport_model(asset.name, error);
  if (!error.empty()) log::error(error);
  if (done > 0) log::info(tr("editor.asset.reimported", {{"count", std::to_string(done)}}));
}

// Writes the sidecar; a model is then imported again wherever the open scene holds it.
void apply(PanelContext& context, AssetState& asset) {
  std::string error;
  if (!ready_to_apply(asset)) return;
  if (!import_settings::save(asset.file, *asset.draft, error)) return log::error(error);
  log::info(tr("editor.asset.saved", {{"file", asset.folder + "/" + asset.name}}));
  if (asset.folder == config::kModelsDirName) reimport(context, asset);
}

void revert(AssetState& asset) {
  asset.problem.clear();
  asset.draft = import_settings::read_checked(asset.file, asset.problem);
}

void draw_problem(const AssetState& asset) {
  if (asset.problem.empty()) return;
  ImGui::PushTextWrapPos();
  ImGui::TextColored(error_color(), "%s", asset.problem.c_str());
  ImGui::PopTextWrapPos();
}

const char* hint_of(const AssetState& asset) {
  if (asset.folder == config::kModelsDirName) return "editor.asset.model_hint";
  return asset.folder == config::kSoundsDirName ? "editor.asset.sound_hint" : "editor.asset.texture_hint";
}

void draw_buttons(PanelContext& context, AssetState& asset) {
  ImGui::Spacing();
  ImGui::BeginDisabled(!context.session.editing());  // (applying a model's settings edits the scene)
  if (ImGui::Button(icon_label(ICON_FA_CHECK, "editor.asset.apply"))) apply(context, asset);
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button(icon_label(ICON_FA_ROTATE_LEFT, "editor.asset.revert"))) revert(asset);
  ImGui::PushTextWrapPos();
  ImGui::TextColored(muted_color(), "%s", tr(hint_of(asset)));
  ImGui::PopTextWrapPos();
}

}  // namespace

bool draw_asset_settings(PanelContext& context) {
  AssetState& asset = context.state.project.asset;
  if (asset.folder.empty()) return false;
  ImGui::TextUnformatted((asset.folder + "/" + asset.name).c_str());
  if (!component_header(ICON_FA_SLIDERS, tr("editor.asset.title"))) return true;
  if (asset.draft) {
    for (const Field& field : import_settings::fields(*asset.draft)) draw_field(asset, field);
  }
  draw_problem(asset);
  if (asset.draft) draw_buttons(context, asset);
  return true;
}

}  // namespace redgem::ui::inspector
