// Inspector card of a terrain: the brush that sculpts it in the Scene view, and paints it (its
// ground layers, when it has them).

#include <algorithm>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;

// The keys of the names of the brush's modes, in SculptMode's order.
constexpr const char* kSculptModes[] = {"editor.inspector.terrain.raise", "editor.inspector.terrain.lower",
                                        "editor.inspector.terrain.smooth", "editor.inspector.terrain.flatten"};
constexpr float kMinBrushRadius = 1.0f;  // meters
constexpr float kMaxBrushRadius = 30.0f;
constexpr float kMinBrushStrength = 0.5f;  // meters per second (see TerrainTool::strength)
constexpr float kMaxBrushStrength = 20.0f;
constexpr const char* kMetersFormat = "%.1f m";
constexpr const char* kSculptId = "##sculpt";
constexpr const char* kSculptingHelp = "editor.inspector.terrain.sculpting_help";  // keys of the texts
constexpr const char* kSculptOffHelp = "editor.inspector.terrain.sculpt_off_help";
constexpr const char* kBrushModeId = "##mode";
constexpr const char* kBrushRadiusId = "##radius";
constexpr const char* kBrushStrengthId = "##strength";
constexpr const char* kLayerId = "##layer";
constexpr const char* kLayerStrengthId = "##layer_strength";
constexpr const char* kFalloffId = "##falloff";
constexpr const char* kLayerHelp = "editor.inspector.terrain.layer_help";
constexpr const char* kShareFormat = "%.2f";

// The editor has a brush, and the object is a terrain to use it on.
bool has_terrain_card(const PanelContext& context, int index) {
  return context.terrain != nullptr && context.session.is_terrain(index);
}

void draw_brush_mode(TerrainTool& tool) {
  property_label(tr("editor.inspector.terrain.brush"));
  ImGui::SetNextItemWidth(kFillWidth);
  choose(kBrushModeId, tool.mode, kSculptModes);
}

void draw_brush_tools(TerrainTool& tool) {
  property_label(tr("editor.inspector.terrain.sculpt"));
  if (ImGui::Checkbox(kSculptId, &tool.sculpting)) tool.painting = false;
  property_label(tr("editor.inspector.terrain.paint"));
  if (ImGui::Checkbox("##paint", &tool.painting)) tool.sculpting = false;
}

// The ground layers of the terrain, when Paint paints them (it tints the vertices of one without).
const TerrainLayers* painted_layers(const PanelContext& context, int index) {
  return context.terrain->painting ? context.session.terrain_layers(index) : nullptr;
}

const char* brush_help(const TerrainTool& tool, const TerrainLayers* layers) {
  if (layers != nullptr) return kLayerHelp;
  if (tool.painting) return "editor.inspector.terrain.paint_help";
  return tool.sculpting ? kSculptingHelp : kSculptOffHelp;
}

void draw_brush_kind(TerrainTool& tool) {
  if (!tool.painting) {
    draw_brush_mode(tool);
    return;
  }
  property_label(tr("editor.inspector.terrain.tint"));
  edit_rgb("##terrain_tint", tool.tint, ImGuiColorEditFlags_Float);
}

void choose_layer(TerrainTool& tool, const TerrainLayers& layers) {
  const int count = static_cast<int>(layers.layers.size());
  tool.layer = std::clamp(tool.layer, 0, count - 1);
  ImGui::SetNextItemWidth(kFillWidth);
  if (!ImGui::BeginCombo(kLayerId, layers.layers[static_cast<std::size_t>(tool.layer)].name.c_str())) return;
  for (int layer = 0; layer < count; ++layer) {
    ImGui::PushID(layer);  // (two layers may have the same name)
    if (ImGui::Selectable(layers.layers[static_cast<std::size_t>(layer)].name.c_str(), layer == tool.layer))
      tool.layer = layer;
    ImGui::PopID();
  }
  ImGui::EndCombo();
}

void draw_share(const char* label, const char* id, float& share) {
  property_label(tr(label));
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(id, &share, 0.0f, 1.0f, kShareFormat);
}

void draw_brush_radius(TerrainTool& tool) {
  property_label(tr("editor.inspector.terrain.radius"));
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushRadiusId, &tool.radius, kMinBrushRadius, kMaxBrushRadius, kMetersFormat);
}

// Paint on a terrain with ground layers: which layer, how strong, how soft, how wide.
void draw_layer_brush(TerrainTool& tool, const TerrainLayers& layers) {
  if (!layers.layers.empty()) {
    property_label(tr("editor.inspector.terrain.layer"));
    choose_layer(tool, layers);
  }
  draw_share("editor.inspector.terrain.strength", kLayerStrengthId, tool.layer_strength);
  draw_share("editor.inspector.terrain.falloff", kFalloffId, tool.falloff);
  draw_brush_radius(tool);
}

void draw_brush_size(TerrainTool& tool) {
  draw_brush_radius(tool);
  property_label(tr("editor.inspector.terrain.strength"));
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushStrengthId, &tool.strength, kMinBrushStrength, kMaxBrushStrength, "%.1f");
}

}  // namespace

void draw_terrain(PanelContext& context, int index) {
  if (!has_terrain_card(context, index)) return;
  if (!component_header(ICON_FA_MOUNTAIN, tr("editor.inspector.terrain.title"))) return;
  TerrainTool& tool = *context.terrain;
  draw_brush_tools(tool);
  const TerrainLayers* layers = painted_layers(context, index);
  ImGui::PushTextWrapPos();
  ImGui::TextColored(muted_color(), "%s", tr(brush_help(tool, layers)));
  ImGui::PopTextWrapPos();
  if (layers != nullptr) return draw_layer_brush(tool, *layers);
  draw_brush_kind(tool);
  draw_brush_size(tool);
}

}  // namespace redgem::ui::inspector
