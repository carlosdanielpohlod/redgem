#include "editor/ui/dpi.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <cmath>

#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {
constexpr float kMinimumScale = 0.5f;
constexpr float kMaximumScale = 4.0f;
void scale_gizmo(float scale) {
  auto& gizmo = ImGuizmo::GetStyle();
  gizmo = ImGuizmo::Style{};
  gizmo.TranslationLineThickness *= scale;
  gizmo.TranslationLineArrowSize *= scale;
  gizmo.RotationLineThickness *= scale;
  gizmo.RotationOuterLineThickness *= scale;
  gizmo.ScaleLineThickness *= scale;
  gizmo.ScaleLineCircleSize *= scale;
  gizmo.HatchedAxisLineThickness *= scale;
  gizmo.CenterCircleSize *= scale;
}
}  // namespace
bool native_dpi_window() {
#ifdef __APPLE__
  return true;
#else
  return false;
#endif
}
float interface_scale(float monitor_scale, bool logical_coordinates) {
  const bool valid = std::isfinite(monitor_scale) && monitor_scale > 0;
  const bool baseline = logical_coordinates || !valid;
  if (baseline) return 1;
  return std::clamp(monitor_scale, kMinimumScale, kMaximumScale);
}
void apply_interface_scale(float scale) {
  auto& style = ImGui::GetStyle();
  style = ImGuiStyle{};
  apply_theme();
  style.ScaleAllSizes(scale);
  style.FontScaleDpi = scale;
  scale_gizmo(scale);
}
float scaled(float pixels) { return pixels * ImGui::GetStyle().FontScaleDpi; }
}  // namespace redgem::ui
