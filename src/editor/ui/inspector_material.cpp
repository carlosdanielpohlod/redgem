// Inspector card of an object's mesh, with the materials it is drawn with.

#include "core/translations.hpp"
#include "editor/editor_logic.hpp"
#include "editor/inspector_logic.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

constexpr float kUnitMin = 0.0f;  // metallic and roughness go from 0 to 1
constexpr float kUnitMax = 1.0f;
constexpr float kNoGlow = 0.0f;  // a glow is never negative
constexpr const char* kBaseColorId = "##base";
constexpr const char* kMetallicId = "##metallic";
constexpr const char* kRoughnessId = "##roughness";
constexpr const char* kEmissionId = "##emission";

// The material's name (or its number) and how many objects show it.
void draw_material_title(const SceneDocument& doc, int material) {
  const std::string& name = doc.material(material).name;
  const std::string title =
      name.empty() ? tr("editor.inspector.material.numbered", {{"number", std::to_string(material)}}) : name;
  ImGui::SeparatorText(title.c_str());
  const int users = doc.material_users(material);
  if (users <= 1) return;
  const std::string shared = tr("editor.inspector.material.shared", {{"count", std::to_string(users)}});
  ImGui::TextColored(muted_color(), ICON_FA_LINK "  %s", shared.c_str());
}

bool draw_material_fields(MaterialDesc& edited) {
  bool changed = false;
  property_label(
      tr(edited.base_color_image != kNoIndex ? "editor.inspector.material.tint" : "editor.inspector.material.color"));
  changed |= edit_color(kBaseColorId, edited.base_color, ImGuiColorEditFlags_Float);
  property_label(tr("editor.inspector.material.metallic"));
  changed |= ImGui::SliderFloat(kMetallicId, &edited.metallic, kUnitMin, kUnitMax);
  property_label(tr("editor.inspector.material.roughness"));
  changed |= ImGui::SliderFloat(kRoughnessId, &edited.roughness, kUnitMin, kUnitMax);
  property_label(tr("editor.inspector.material.emission"));
  changed |= edit_rgb(kEmissionId, edited.emissive, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
  tooltip(tr("editor.inspector.material.emission_tooltip"));
  return changed;
}

void draw_texture_note(const MaterialDesc& material) {
  if (!inspector_logic::textured(material)) return;
  const char* key = material.normal_image != kNoIndex ? "editor.inspector.material.textured_with_normal_map"
                                                      : "editor.inspector.material.textured";
  ImGui::TextColored(muted_color(), "%s", icon_label(ICON_FA_IMAGE, key));
}

void draw_material(PanelContext& context, int material) {
  SceneDocument& doc = context.session.document();
  MaterialDesc edited = doc.material(material);
  ImGui::PushID(material);
  draw_material_title(doc, material);
  const bool changed = draw_material_fields(edited);
  draw_texture_note(edited);
  if (changed) {
    streamed_edit(context);
    edited.emissive = editor_logic::at_least(edited.emissive, kNoGlow);
    doc.set_material(material, edited, false);
  }
  ImGui::PopID();
}

}  // namespace

void draw_mesh(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  if (node.mesh == kNoIndex) return;
  if (!component_header(ICON_FA_CUBE, tr("editor.inspector.mesh.title"))) return;
  const MeshDesc& mesh = context.session.scene_desc().meshes[node.mesh];
  property_label(tr("editor.inspector.mesh.label"));
  ImGui::TextUnformatted(mesh.name.empty() ? tr("editor.inspector.mesh.unnamed") : mesh.name.c_str());
  for (const int material : editor_logic::materials_of(mesh)) draw_material(context, material);
}

}  // namespace redgem::ui::inspector
