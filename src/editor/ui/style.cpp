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

// Whether a window is the named one or one of its own children (a text box's scrolling area).
// With no window to place the active item, it counts as outside.
bool inside(const ImGuiWindow* window, const ImGuiWindow* named) {
  if (window == nullptr || named == nullptr) return false;
  return window->RootWindow == named->RootWindow;
}

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
  ImGui::PushID(tip);  // buttons sharing an icon (the panel toggles) stay apart by what they do
  const bool pressed = ImGui::Button(icon, ImVec2(side * kIconButtonWidth, side));
  ImGui::PopID();
  if (active) ImGui::PopStyleColor(kActiveButtonColors);
  tooltip(tip);
  return pressed;
}

void tooltip(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", text);
  }
}

bool item_active_outside(const char* window_name) {
  if (!ImGui::IsAnyItemActive()) return false;
  return !inside(GImGui->ActiveIdWindow, ImGui::FindWindowByName(window_name));
}

void toggle_button(const char* icon, const char* tip, bool& on) {
  if (icon_button(icon, tip, on)) on = !on;
}

const char* dropped_text(const char* payload_type) {
  const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payload_type);
  return payload != nullptr ? static_cast<const char*>(payload->Data) : nullptr;
}

}  // namespace redgem::ui
