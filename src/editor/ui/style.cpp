#include "editor/ui/style.hpp"

#include <imgui_internal.h>

#include "editor/editor_config.hpp"
#include "editor/ui/embedded_fonts.hpp"
#include "render/ui_font.hpp"
#include "extras/FA6FreeSolidFontData.h"

namespace redgem::ui {
namespace {

using namespace editor_config;

// The fonts, in the order load_fonts adds them to the interface's own list.
constexpr int kCodeFontSlot = 1;  // after the interface's font, which is the default

// Icons are a bit smaller than the text and nudged down to sit on its baseline.
constexpr float kIconScale = 0.9f;
constexpr float kIconOffsetY = 1.0f;
constexpr float kIconMinAdvance = 1.25f;  // icons get a fixed width (in font sizes) so they line up
constexpr ImWchar kRangesEnd = 0;         // ImGui: closes a list of glyph ranges

// Palette: neutral slate grays with a ruby accent.
constexpr ImVec4 kText{0.90f, 0.91f, 0.93f, 1.0f};
constexpr ImVec4 kTextDisabled{0.49f, 0.51f, 0.56f, 1.0f};
constexpr ImVec4 kWindow{0.106f, 0.114f, 0.133f, 1.0f};
constexpr ImVec4 kPanelDark{0.082f, 0.090f, 0.106f, 1.0f};
constexpr ImVec4 kPopup{0.133f, 0.145f, 0.169f, 0.98f};
constexpr ImVec4 kBorder{0.180f, 0.192f, 0.220f, 1.0f};
constexpr ImVec4 kFrame{0.165f, 0.176f, 0.204f, 1.0f};
constexpr ImVec4 kFrameHover{0.200f, 0.216f, 0.247f, 1.0f};
constexpr ImVec4 kFrameActive{0.231f, 0.247f, 0.282f, 1.0f};
constexpr ImVec4 kHeader{0.169f, 0.180f, 0.212f, 1.0f};
constexpr ImVec4 kHeaderHover{0.216f, 0.231f, 0.271f, 1.0f};
constexpr ImVec4 kAccent{0.878f, 0.239f, 0.302f, 1.0f};
constexpr ImVec4 kAccentHover{0.937f, 0.341f, 0.400f, 1.0f};
constexpr ImVec4 kAccentSoft{0.878f, 0.239f, 0.302f, 0.35f};
constexpr ImVec4 kAccentFaint{0.878f, 0.239f, 0.302f, 0.18f};
constexpr ImVec4 kScrollbar{0.231f, 0.247f, 0.282f, 1.0f};
constexpr ImVec4 kTransparent{0.0f, 0.0f, 0.0f, 0.0f};
constexpr ImVec4 kModalDim{0.0f, 0.0f, 0.0f, 0.55f};
constexpr ImVec4 kMuted{0.58f, 0.60f, 0.66f, 1.0f};
constexpr ImVec4 kError{1.0f, 0.42f, 0.42f, 1.0f};
constexpr ImVec4 kWarning{1.0f, 0.78f, 0.32f, 1.0f};
constexpr ImVec4 kSuccess{0.40f, 0.85f, 0.55f, 1.0f};

// Spacing and shapes.
constexpr ImVec2 kWindowPadding{10.0f, 10.0f};
constexpr ImVec2 kFramePadding{8.0f, 5.0f};
constexpr ImVec2 kItemSpacing{8.0f, 6.0f};
constexpr ImVec2 kItemInnerSpacing{6.0f, 4.0f};
constexpr ImVec2 kCellPadding{6.0f, 4.0f};
constexpr float kIndentSpacing = 16.0f;
constexpr float kScrollbarSize = 12.0f;
constexpr float kGrabMinSize = 10.0f;
constexpr float kSmallRounding = 4.0f;
constexpr float kLargeRounding = 6.0f;
constexpr float kScrollbarRounding = 8.0f;
constexpr float kBorderSize = 1.0f;
constexpr float kTabBarOverline = 2.0f;
constexpr float kTreeLinesSize = 1.0f;
constexpr float kSeparatorTextPadding = 0.0f;
constexpr float kDisabledAlpha = 0.45f;
constexpr int kActiveButtonColors = 2;                     // style colors icon_button pushes for a pressed button
constexpr const char* kHeaderLabelFormat = "%s  %s###%s";  // icon, title, and the title again as the id

// The icons, merged into the font added last.
void merge_icons(float size) {
  static const ImWchar kIconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, kRangesEnd};
  ImFontConfig icons;
  icons.MergeMode = true;
  icons.PixelSnapH = true;
  icons.FontDataOwnedByAtlas = false;
  icons.GlyphMinAdvanceX = size * kIconMinAdvance;
  icons.GlyphOffset = ImVec2(0.0f, kIconOffsetY);
  ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size,
                                                       size * kIconScale, &icons, kIconRanges);
}

void add_font(const EmbeddedFile& font, float size) {
  ImFontConfig config;
  config.FontDataOwnedByAtlas = false;  // the data lives in the binary
  ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(font.data), font.size, size, &config);
  merge_icons(size);
}

// Which color of the palette each part of the interface gets.
struct ThemeColor {
  ImGuiCol slot;
  ImVec4 color;
};
constexpr ThemeColor kThemeColors[] = {
    {ImGuiCol_Text, kText},
    {ImGuiCol_TextDisabled, kTextDisabled},
    {ImGuiCol_WindowBg, kWindow},
    {ImGuiCol_ChildBg, kTransparent},
    {ImGuiCol_PopupBg, kPopup},
    {ImGuiCol_Border, kBorder},
    {ImGuiCol_BorderShadow, kTransparent},
    {ImGuiCol_FrameBg, kFrame},
    {ImGuiCol_FrameBgHovered, kFrameHover},
    {ImGuiCol_FrameBgActive, kFrameActive},
    {ImGuiCol_TitleBg, kPanelDark},
    {ImGuiCol_TitleBgActive, kPanelDark},
    {ImGuiCol_TitleBgCollapsed, kPanelDark},
    {ImGuiCol_MenuBarBg, kPanelDark},
    {ImGuiCol_ScrollbarBg, kTransparent},
    {ImGuiCol_ScrollbarGrab, kScrollbar},
    {ImGuiCol_ScrollbarGrabHovered, kHeaderHover},
    {ImGuiCol_ScrollbarGrabActive, kAccentSoft},
    {ImGuiCol_CheckMark, kAccent},
    {ImGuiCol_SliderGrab, kAccent},
    {ImGuiCol_SliderGrabActive, kAccentHover},
    {ImGuiCol_Button, kFrame},
    {ImGuiCol_ButtonHovered, kFrameHover},
    {ImGuiCol_ButtonActive, kAccentSoft},
    {ImGuiCol_Header, kHeader},
    {ImGuiCol_HeaderHovered, kHeaderHover},
    {ImGuiCol_HeaderActive, kAccentSoft},
    {ImGuiCol_Separator, kBorder},
    {ImGuiCol_SeparatorHovered, kAccentSoft},
    {ImGuiCol_SeparatorActive, kAccent},
    {ImGuiCol_ResizeGrip, kTransparent},
    {ImGuiCol_ResizeGripHovered, kAccentSoft},
    {ImGuiCol_ResizeGripActive, kAccent},
    {ImGuiCol_InputTextCursor, kText},
    {ImGuiCol_Tab, kPanelDark},
    {ImGuiCol_TabHovered, kHeaderHover},
    {ImGuiCol_TabSelected, kWindow},
    {ImGuiCol_TabSelectedOverline, kAccent},
    {ImGuiCol_TabDimmed, kPanelDark},
    {ImGuiCol_TabDimmedSelected, kWindow},
    {ImGuiCol_TabDimmedSelectedOverline, kBorder},
    {ImGuiCol_DockingPreview, kAccentSoft},
    {ImGuiCol_DockingEmptyBg, kPanelDark},
    {ImGuiCol_TableHeaderBg, kHeader},
    {ImGuiCol_TableBorderStrong, kBorder},
    {ImGuiCol_TableBorderLight, kBorder},
    {ImGuiCol_TextSelectedBg, kAccentSoft},
    {ImGuiCol_TextLink, kAccentHover},
    {ImGuiCol_TreeLines, kBorder},
    {ImGuiCol_DragDropTarget, kAccent},
    {ImGuiCol_DragDropTargetBg, kAccentFaint},
    {ImGuiCol_NavCursor, kAccent},
    {ImGuiCol_ModalWindowDimBg, kModalDim},
    {ImGuiCol_UnsavedMarker, kAccent},
};

void apply_spacing(ImGuiStyle& style) {
  style.WindowPadding = kWindowPadding;
  style.FramePadding = kFramePadding;
  style.ItemSpacing = kItemSpacing;
  style.ItemInnerSpacing = kItemInnerSpacing;
  style.CellPadding = kCellPadding;
  style.IndentSpacing = kIndentSpacing;
  style.ScrollbarSize = kScrollbarSize;
  style.GrabMinSize = kGrabMinSize;
  style.SeparatorTextPadding.x = kSeparatorTextPadding;
}

void apply_rounding(ImGuiStyle& style) {
  style.WindowRounding = kLargeRounding;
  style.ChildRounding = kSmallRounding;
  style.FrameRounding = kSmallRounding;
  style.PopupRounding = kLargeRounding;
  style.GrabRounding = kSmallRounding;
  style.TabRounding = kSmallRounding;
  style.ScrollbarRounding = kScrollbarRounding;
}

void apply_borders(ImGuiStyle& style) {
  style.WindowBorderSize = kBorderSize;
  style.PopupBorderSize = kBorderSize;
  style.FrameBorderSize = 0.0f;
  style.TabBorderSize = 0.0f;
  style.TabBarOverlineSize = kTabBarOverline;
  style.DockingSeparatorSize = kBorderSize;
  style.WindowMenuButtonPosition = ImGuiDir_None;  // docked panels show just their title
  style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;
  style.TreeLinesSize = kTreeLinesSize;
  style.DisabledAlpha = kDisabledAlpha;
}

}  // namespace

void load_fonts() {
  add_font(embedded_ui_font(), kInterfaceFontSize);  // the first font is the default
  add_font(fonts::code_font(), kCodeFontSize);       // kCodeFontSlot
}

ImFont* code_font() { return ImGui::GetIO().Fonts->Fonts[kCodeFontSlot]; }

void apply_theme() {
  ImGuiStyle& style = ImGui::GetStyle();
  apply_spacing(style);
  apply_rounding(style);
  apply_borders(style);
  for (const ThemeColor& entry : kThemeColors) style.Colors[entry.slot] = entry.color;
}

ImVec4 accent_color() { return kAccent; }
ImVec4 muted_color() { return kMuted; }
ImVec4 error_color() { return kError; }
ImVec4 warning_color() { return kWarning; }
ImVec4 success_color() { return kSuccess; }

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
