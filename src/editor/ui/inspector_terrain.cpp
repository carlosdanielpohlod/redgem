// Inspector card of a terrain: the brush that sculpts it in the Scene view.

#include "editor/editor_config.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;

constexpr const char* kSculptModes[] = {"Raise", "Lower", "Smooth", "Flatten"};  // in SculptMode's order
constexpr float kMinBrushRadius = 1.0f;                                          // meters
constexpr float kMaxBrushRadius = 30.0f;
constexpr float kMinBrushStrength = 0.5f;  // meters per second (see TerrainTool::strength)
constexpr float kMaxBrushStrength = 20.0f;
constexpr const char* kMetersFormat = "%.1f m";
constexpr const char* kSculptId = "##sculpt";
constexpr const char* kSculptingHelp =
    "Drag the left mouse button over the terrain in the Scene view. Shift reverses Raise and Lower.";
constexpr const char* kSculptOffHelp = "Turn Sculpt on to shape the ground with the mouse: mountains, holes, plateaus.";
constexpr const char* kBrushModeId = "##mode";
constexpr const char* kBrushRadiusId = "##radius";
constexpr const char* kBrushStrengthId = "##strength";

// The editor has a brush, and the object is a terrain to use it on.
bool has_terrain_card(const PanelContext& context, int index) {
  return context.terrain != nullptr && context.session.is_terrain(index);
}

void draw_brush_mode(TerrainTool& tool) {
  property_label("Brush");
  ImGui::SetNextItemWidth(kFillWidth);
  choose(kBrushModeId, tool.mode, kSculptModes);
}

void draw_brush_size(TerrainTool& tool) {
  property_label("Radius");
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushRadiusId, &tool.radius, kMinBrushRadius, kMaxBrushRadius, kMetersFormat);
  property_label("Strength");
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushStrengthId, &tool.strength, kMinBrushStrength, kMaxBrushStrength, "%.1f");
}

}  // namespace

void draw_terrain(PanelContext& context, int index) {
  if (!has_terrain_card(context, index)) return;
  if (!component_header(ICON_FA_MOUNTAIN, "Terrain")) return;
  TerrainTool& tool = *context.terrain;
  property_label("Sculpt");
  ImGui::Checkbox(kSculptId, &tool.sculpting);
  ImGui::PushTextWrapPos();
  ImGui::TextColored(muted_color(), "%s", tool.sculpting ? kSculptingHelp : kSculptOffHelp);
  ImGui::PopTextWrapPos();
  draw_brush_mode(tool);
  draw_brush_size(tool);
}

}  // namespace redgem::ui::inspector
