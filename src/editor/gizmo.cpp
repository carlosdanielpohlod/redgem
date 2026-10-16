#include "editor/gizmo.hpp"

#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <ImGuizmo.h>

#include <iterator>

#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/scene_viewport.hpp"
#include "editor/ui/style.hpp"
#include "render/render_math.hpp"

namespace redgem {
namespace {

using namespace editor_config;
using render_math::model_matrix;

// What the gizmo can do: the key that picks each mode and its toolbar button.
struct GizmoTool {
  GizmoMode mode;
  int key;
  const char* icon;
  const char* tooltip;  // the key of its text
};
constexpr GizmoTool kGizmoTools[] = {
    {GizmoMode::Translate, KEY_W, ICON_FA_UP_DOWN_LEFT_RIGHT, "editor.toolbar.move"},
    {GizmoMode::Rotate, KEY_E, ICON_FA_ROTATE, "editor.toolbar.rotate"},
    {GizmoMode::Scale, KEY_R, ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, "editor.toolbar.scale"},
};

ImGuizmo::OPERATION gizmo_operation(GizmoMode mode) {
  if (mode == GizmoMode::Translate) return ImGuizmo::TRANSLATE;
  return mode == GizmoMode::Rotate ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
}

}  // namespace

float snap_step(GizmoMode mode) {
  if (mode == GizmoMode::Translate) return kTranslateSnap;
  return mode == GizmoMode::Rotate ? kRotateSnapDegrees : kScaleSnap;
}

void Gizmo::use_keys() {
  for (const GizmoTool& tool : kGizmoTools) {
    if (IsKeyPressed(tool.key)) mode_ = tool.mode;
  }
}

void Gizmo::draw_tools() {
  for (const GizmoTool& tool : kGizmoTools) {
    if (&tool != std::begin(kGizmoTools)) ImGui::SameLine();
    if (ui::icon_button(tool.icon, tr(tool.tooltip), mode_ == tool.mode)) mode_ = tool.mode;
  }
  ImGui::SameLine(0.0f, kToolbarGroupGap);
  if (ui::icon_button(local_ ? ICON_FA_CUBE : ICON_FA_GLOBE,
                      tr(local_ ? "editor.toolbar.local_axes" : "editor.toolbar.world_axes"))) {
    local_ = !local_;
  }
  ImGui::SameLine();
  ui::toggle_button(ICON_FA_MAGNET, tr("editor.toolbar.snap"), snapping_);
}

bool Gizmo::snaps() const { return snapping_ || ImGui::GetIO().KeyCtrl; }

std::optional<Transform> Gizmo::manipulate(const Transform& world, const SceneViewport& view) {
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(view.screen_x, view.screen_y, view.width_pixels(), view.height_pixels());
  float16 model = MatrixToFloatV(model_matrix(world));
  drag(model.v, view);
  if (!ImGuizmo::IsUsing()) return std::nullopt;
  return transform_from_matrix(model.v);
}

void Gizmo::drag(float* model, const SceneViewport& view) const {
  const float16 camera = MatrixToFloatV(GetCameraMatrix(view.camera));
  // Same projection as raylib's BeginMode3D.
  const float16 projection =
      MatrixToFloatV(MatrixPerspective(view.camera.fovy * DEG2RAD, view.width_pixels() / view.height_pixels(),
                                       rlGetCullDistanceNear(), rlGetCullDistanceFar()));
  const float step = snap_step(mode_);
  const float snap_values[] = {step, step, step};
  ImGuizmo::Manipulate(camera.v, projection.v, gizmo_operation(mode_), own_axes() ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
                       model, nullptr, snaps() ? snap_values : nullptr);
}

}  // namespace redgem
