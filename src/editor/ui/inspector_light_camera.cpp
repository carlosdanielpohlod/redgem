// Inspector cards of a light and of a camera.

#include <algorithm>
#include <cfloat>

#include "editor/editor_config.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;

constexpr const char* kLightTypes[] = {"Directional", "Point", "Spot"};  // in LightType's order
constexpr float kRadiansToDegrees = 57.29577951f;
constexpr float kNoLowerLimit = 0.0f;            // intensities, ranges and clip distances are never negative
constexpr const char* kMetersFormat = "%.1f m";  // how distances are shown in drags
constexpr const char* kTypeId = "##type";
constexpr const char* kColorId = "##color";
constexpr const char* kIntensityId = "##intensity";
constexpr const char* kRangeId = "##range";
constexpr const char* kFovId = "##fov";
constexpr const char* kNearId = "##near";
constexpr const char* kFarId = "##far";

bool draw_light_range(LightDesc& light) {
  property_label("Range");
  const bool changed =
      ImGui::DragFloat(kRangeId, &light.range, kDragSpeedNumber, kNoLowerLimit, FLT_MAX, kMetersFormat);
  tooltip("0 = not set (the engine then uses its default reach)");
  return changed;
}

bool draw_light_fields(LightDesc& light) {
  const bool is_sun = light.type == LightType::Directional;  // as it was before this frame's edits
  bool changed = false;
  property_label("Type");
  changed |= choose(kTypeId, light.type, kLightTypes);
  property_label("Color");
  changed |= edit_rgb(kColorId, light.color, ImGuiColorEditFlags_Float);
  property_label("Intensity");
  changed |= ImGui::DragFloat(kIntensityId, &light.intensity, kDragSpeedIntensity, kNoLowerLimit, FLT_MAX);
  tooltip(is_sun ? "Sun strength" : "Watts (like Blender)");
  if (!is_sun) changed |= draw_light_range(light);
  return changed;
}

bool draw_camera_fields(CameraDesc& camera, float& fov_degrees) {
  bool changed = false;
  property_label("Field of view");
  changed |= ImGui::SliderFloat(kFovId, &fov_degrees, kMinFovDegrees, kMaxFovDegrees, "%.0f deg");
  property_label("Near");
  changed |= ImGui::DragFloat(kNearId, &camera.near_plane, kDragSpeedScale, kMinClipDistance, FLT_MAX);
  property_label("Far");
  changed |= ImGui::DragFloat(kFarId, &camera.far_plane, kDragSpeedNumber, kNoLowerLimit, FLT_MAX);
  tooltip("0 = infinite");
  return changed;
}

void draw_main_camera_note(const EditorSession& session, int index) {
  if (session.scene().main_camera() != index) return;
  ImGui::TextColored(muted_color(), ICON_FA_STAR "  The game starts with this camera");
}

}  // namespace

void draw_light(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  if (!node.light) return;
  LightDesc light = *node.light;
  if (!component_header(ICON_FA_LIGHTBULB, "Light")) return;
  if (!draw_light_fields(light)) return;
  streamed_edit(context);
  light.intensity = std::max(light.intensity, kNoLowerLimit);
  light.range = std::max(light.range, kNoLowerLimit);
  context.session.document().set_light(index, light, false);
  context.session.refresh_preview();
}

void draw_camera(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  if (!node.camera) return;
  CameraDesc camera = *node.camera;
  if (!component_header(ICON_FA_VIDEO, "Camera")) return;
  float fov = camera.fov_y_radians * kRadiansToDegrees;
  const bool changed = draw_camera_fields(camera, fov);
  draw_main_camera_note(context.session, index);
  if (!changed) return;
  streamed_edit(context);
  camera.fov_y_radians = std::clamp(fov, kMinFovDegrees, kMaxFovDegrees) / kRadiansToDegrees;
  camera.near_plane = std::max(camera.near_plane, kMinClipDistance);
  context.session.document().set_camera(index, camera, false);
  context.session.refresh_preview();
}

}  // namespace redgem::ui::inspector
