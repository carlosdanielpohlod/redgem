// The dark editor theme: which color of the palette each part of the interface gets, and its
// spacing, rounding and borders.

#include "editor/ui/palette.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

using namespace palette;

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

}  // namespace redgem::ui
