// Inspector card of a terrain: the brush that sculpts it in the Scene view.

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

// The editor has a brush, and the object is a terrain to use it on.
bool has_terrain_card(const PanelContext& context, int index) {
  return context.terrain != nullptr && context.session.is_terrain(index);
}

void draw_brush_mode(TerrainTool& tool) {
  property_label(tr("editor.inspector.terrain.brush"));
  ImGui::SetNextItemWidth(kFillWidth);
  choose(kBrushModeId, tool.mode, kSculptModes);
}

void draw_brush_size(TerrainTool& tool) {
  property_label(tr("editor.inspector.terrain.radius"));
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushRadiusId, &tool.radius, kMinBrushRadius, kMaxBrushRadius, kMetersFormat);
  property_label(tr("editor.inspector.terrain.strength"));
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushStrengthId, &tool.strength, kMinBrushStrength, kMaxBrushStrength, "%.1f");
}

}  // namespace

void draw_terrain(PanelContext& context, int index) {
  if (!has_terrain_card(context, index)) return;
  if (!component_header(ICON_FA_MOUNTAIN, tr("editor.inspector.terrain.title"))) return;
  TerrainTool& tool = *context.terrain;
  property_label(tr("editor.inspector.terrain.sculpt"));
  ImGui::Checkbox(kSculptId, &tool.sculpting);
  ImGui::PushTextWrapPos();
  ImGui::TextColored(muted_color(), "%s", tr(tool.sculpting ? kSculptingHelp : kSculptOffHelp));
  ImGui::PopTextWrapPos();
  draw_brush_mode(tool);
  draw_brush_size(tool);
}

}  // namespace redgem::ui::inspector
