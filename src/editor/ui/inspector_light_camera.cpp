// Inspector cards of a light and of a camera.

#include <algorithm>
#include <cfloat>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;

// The keys of the names of the kinds of light, in LightType's order.
constexpr const char* kLightTypes[] = {"editor.inspector.light.directional", "editor.inspector.light.point",
                                       "editor.inspector.light.spot"};
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
  property_label(tr("editor.inspector.light.range"));
  const bool changed =
      ImGui::DragFloat(kRangeId, &light.range, kDragSpeedNumber, kNoLowerLimit, FLT_MAX, kMetersFormat);
  tooltip(tr("editor.inspector.light.range_tooltip"));
  return changed;
}

bool draw_light_fields(LightDesc& light) {
  const bool is_sun = light.type == LightType::Directional;  // as it was before this frame's edits
  bool changed = false;
  property_label(tr("editor.inspector.light.type"));
  changed |= choose(kTypeId, light.type, kLightTypes);
  property_label(tr("editor.inspector.light.color"));
  changed |= edit_rgb(kColorId, light.color, ImGuiColorEditFlags_Float);
  property_label(tr("editor.inspector.light.intensity"));
  changed |= ImGui::DragFloat(kIntensityId, &light.intensity, kDragSpeedIntensity, kNoLowerLimit, FLT_MAX);
  tooltip(tr(is_sun ? "editor.inspector.light.sun_strength" : "editor.inspector.light.watts"));
  if (!is_sun) changed |= draw_light_range(light);
  return changed;
}

bool draw_camera_fields(CameraDesc& camera, float& fov_degrees) {
  bool changed = false;
  property_label(tr("editor.inspector.camera.field_of_view"));
  changed |= ImGui::SliderFloat(kFovId, &fov_degrees, kMinFovDegrees, kMaxFovDegrees, "%.0f deg");
  property_label(tr("editor.inspector.camera.near"));
  changed |= ImGui::DragFloat(kNearId, &camera.near_plane, kDragSpeedScale, kMinClipDistance, FLT_MAX);
  property_label(tr("editor.inspector.camera.far"));
  changed |= ImGui::DragFloat(kFarId, &camera.far_plane, kDragSpeedNumber, kNoLowerLimit, FLT_MAX);
  tooltip(tr("editor.inspector.camera.far_tooltip"));
  return changed;
}

void draw_main_camera_note(const EditorSession& session, int index) {
  if (session.scene().main_camera() != index) return;
  ImGui::TextColored(muted_color(), "%s", icon_label(ICON_FA_STAR, "editor.inspector.camera.main"));
}

}  // namespace

void draw_light(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  if (!node.light) return;
  LightDesc light = *node.light;
  if (!component_header(ICON_FA_LIGHTBULB, tr("editor.inspector.light.title"))) return;
  if (!draw_light_fields(light)) return;
  streamed_edit(context);
  light.intensity = std::max(light.intensity, kNoLowerLimit);
  light.range = std::max(light.range, kNoLowerLimit);
  context.session.document().set_light(index, light, false);
  context.session.refresh_node(index);
}

void draw_camera(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  if (!node.camera) return;
  CameraDesc camera = *node.camera;
  if (!component_header(ICON_FA_VIDEO, tr("editor.inspector.camera.title"))) return;
  float fov = camera.fov_y_radians * kRadiansToDegrees;
  const bool changed = draw_camera_fields(camera, fov);
  draw_main_camera_note(context.session, index);
  if (!changed) return;
  streamed_edit(context);
  camera.fov_y_radians = std::clamp(fov, kMinFovDegrees, kMaxFovDegrees) / kRadiansToDegrees;
  camera.near_plane = std::max(camera.near_plane, kMinClipDistance);
  context.session.document().set_camera(index, camera, false);
  context.session.refresh_node(index);
}

}  // namespace redgem::ui::inspector
