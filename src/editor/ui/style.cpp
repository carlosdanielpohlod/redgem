// The widgets the panels are made of.

#include "editor/ui/style.hpp"

#include <imgui_internal.h>

#include "editor/editor_config.hpp"
#include "editor/ui/palette.hpp"

namespace redgem::ui {
namespace {

using namespace editor_config;
using namespace palette;

constexpr int kActiveButtonColors = 2;                     // style colors icon_button pushes for a pressed button
constexpr const char* kHeaderLabelFormat = "%s  %s###%s";  // icon, title, and the title again as the id

}  // namespace

void property_label(const char* text) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(kMuted, "%s", text);
  ImGui::SameLine(kLabelColumnWidth);
  ImGui::SetNextItemWidth(kFillWidth);
}

bool component_header(const char* icon, const char* title, bool* removed) {
  ImGui::Spacing();
  char label[kTextBufferSize];
  ImFormatString(label, sizeof(label), kHeaderLabelFormat, icon, title, title);
  bool visible = true;
  const bool open = ImGui::CollapsingHeader(label, removed != nullptr ? &visible : nullptr,
                                            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
  if (removed != nullptr) *removed = !visible;
  return open;
}

bool icon_button(const char* icon, const char* tip, bool active) {
  if (active) {
    ImGui::PushStyleColor(ImGuiCol_Button, kAccentSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentSoft);
  }
  const float side = ImGui::GetFrameHeight();
  const bool pressed = ImGui::Button(icon, ImVec2(side * kIconButtonWidth, side));
  if (active) ImGui::PopStyleColor(kActiveButtonColors);
  tooltip(tip);
  return pressed;
}

void tooltip(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", text);
  }
}

void toggle_button(const char* icon, const char* tip, bool& on) {
  if (icon_button(icon, tip, on)) on = !on;
}

const char* dropped_text(const char* payload_type) {
  const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payload_type);
  return payload != nullptr ? static_cast<const char*>(payload->Data) : nullptr;
}

}  // namespace redgem::ui
