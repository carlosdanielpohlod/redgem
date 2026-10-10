#include "editor/ui/style.hpp"

#include <imgui_internal.h>

#include "editor/editor_config.hpp"
#include "editor/ui/embedded_fonts.hpp"
#include "extras/FA6FreeSolidFontData.h"

namespace redgem::ui {
namespace {

using namespace editor_config;

ImFont* g_code_font = nullptr;

// Icons are a bit smaller than the text and nudged down to sit on its baseline.
constexpr float kIconScale = 0.9f;
constexpr float kIconOffsetY = 1.0f;
constexpr float kIconMinAdvance = 1.25f;  // icons get a fixed width (in font sizes) so they line up

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

ImFont* add_font(const fonts::EmbeddedFont& font, float size) {
  ImGuiIO& io = ImGui::GetIO();
  ImFontConfig config;
  config.FontDataOwnedByAtlas = false;  // the data lives in the binary
  ImFont* added = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(font.data), font.size, size, &config);

  static const ImWchar kIconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
  ImFontConfig icons;
  icons.MergeMode = true;
  icons.PixelSnapH = true;
  icons.FontDataOwnedByAtlas = false;
  icons.GlyphMinAdvanceX = size * kIconMinAdvance;
  icons.GlyphOffset = ImVec2(0.0f, kIconOffsetY);
  io.Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size,
                                           size * kIconScale, &icons, kIconRanges);
  return added;
}

}  // namespace

void load_fonts() {
  add_font(fonts::interface_font(), kInterfaceFontSize);  // the first font is the default
  g_code_font = add_font(fonts::code_font(), kCodeFontSize);
}

ImFont* code_font() { return g_code_font; }

void apply_theme() {
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowPadding = kWindowPadding;
  style.FramePadding = kFramePadding;
  style.ItemSpacing = kItemSpacing;
  style.ItemInnerSpacing = kItemInnerSpacing;
  style.CellPadding = kCellPadding;
  style.IndentSpacing = kIndentSpacing;
  style.ScrollbarSize = kScrollbarSize;
  style.GrabMinSize = kGrabMinSize;
  style.WindowRounding = kLargeRounding;
  style.ChildRounding = kSmallRounding;
  style.FrameRounding = kSmallRounding;
  style.PopupRounding = kLargeRounding;
  style.GrabRounding = kSmallRounding;
  style.TabRounding = kSmallRounding;
  style.ScrollbarRounding = kScrollbarRounding;
  style.WindowBorderSize = kBorderSize;
  style.PopupBorderSize = kBorderSize;
  style.FrameBorderSize = 0.0f;
  style.TabBorderSize = 0.0f;
  style.TabBarOverlineSize = kTabBarOverline;
  style.DockingSeparatorSize = kBorderSize;
  style.WindowMenuButtonPosition = ImGuiDir_None;  // docked panels show just their title
  style.SeparatorTextPadding.x = kSeparatorTextPadding;
  style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;
  style.TreeLinesSize = kTreeLinesSize;
  style.DisabledAlpha = kDisabledAlpha;

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = kText;
  colors[ImGuiCol_TextDisabled] = kTextDisabled;
  colors[ImGuiCol_WindowBg] = kWindow;
  colors[ImGuiCol_ChildBg] = kTransparent;
  colors[ImGuiCol_PopupBg] = kPopup;
  colors[ImGuiCol_Border] = kBorder;
  colors[ImGuiCol_BorderShadow] = kTransparent;
  colors[ImGuiCol_FrameBg] = kFrame;
  colors[ImGuiCol_FrameBgHovered] = kFrameHover;
  colors[ImGuiCol_FrameBgActive] = kFrameActive;
  colors[ImGuiCol_TitleBg] = kPanelDark;
  colors[ImGuiCol_TitleBgActive] = kPanelDark;
  colors[ImGuiCol_TitleBgCollapsed] = kPanelDark;
  colors[ImGuiCol_MenuBarBg] = kPanelDark;
  colors[ImGuiCol_ScrollbarBg] = kTransparent;
  colors[ImGuiCol_ScrollbarGrab] = kScrollbar;
  colors[ImGuiCol_ScrollbarGrabHovered] = kHeaderHover;
  colors[ImGuiCol_ScrollbarGrabActive] = kAccentSoft;
  colors[ImGuiCol_CheckMark] = kAccent;
  colors[ImGuiCol_SliderGrab] = kAccent;
  colors[ImGuiCol_SliderGrabActive] = kAccentHover;
  colors[ImGuiCol_Button] = kFrame;
  colors[ImGuiCol_ButtonHovered] = kFrameHover;
  colors[ImGuiCol_ButtonActive] = kAccentSoft;
  colors[ImGuiCol_Header] = kHeader;
  colors[ImGuiCol_HeaderHovered] = kHeaderHover;
  colors[ImGuiCol_HeaderActive] = kAccentSoft;
  colors[ImGuiCol_Separator] = kBorder;
  colors[ImGuiCol_SeparatorHovered] = kAccentSoft;
  colors[ImGuiCol_SeparatorActive] = kAccent;
  colors[ImGuiCol_ResizeGrip] = kTransparent;
  colors[ImGuiCol_ResizeGripHovered] = kAccentSoft;
  colors[ImGuiCol_ResizeGripActive] = kAccent;
  colors[ImGuiCol_InputTextCursor] = kText;
  colors[ImGuiCol_Tab] = kPanelDark;
  colors[ImGuiCol_TabHovered] = kHeaderHover;
  colors[ImGuiCol_TabSelected] = kWindow;
  colors[ImGuiCol_TabSelectedOverline] = kAccent;
  colors[ImGuiCol_TabDimmed] = kPanelDark;
  colors[ImGuiCol_TabDimmedSelected] = kWindow;
  colors[ImGuiCol_TabDimmedSelectedOverline] = kBorder;
  colors[ImGuiCol_DockingPreview] = kAccentSoft;
  colors[ImGuiCol_DockingEmptyBg] = kPanelDark;
  colors[ImGuiCol_TableHeaderBg] = kHeader;
  colors[ImGuiCol_TableBorderStrong] = kBorder;
  colors[ImGuiCol_TableBorderLight] = kBorder;
  colors[ImGuiCol_TextSelectedBg] = kAccentSoft;
  colors[ImGuiCol_TextLink] = kAccentHover;
  colors[ImGuiCol_TreeLines] = kBorder;
  colors[ImGuiCol_DragDropTarget] = kAccent;
  colors[ImGuiCol_DragDropTargetBg] = kAccentFaint;
  colors[ImGuiCol_NavCursor] = kAccent;
  colors[ImGuiCol_ModalWindowDimBg] = kModalDim;
  colors[ImGuiCol_UnsavedMarker] = kAccent;
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
  ImGui::SetNextItemWidth(-1.0f);
}

bool component_header(const char* icon, const char* title, bool* removed) {
  ImGui::Spacing();
  char label[kTextBufferSize];
  ImFormatString(label, sizeof(label), "%s  %s###%s", icon, title, title);
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
  if (active) ImGui::PopStyleColor(2);
  tooltip(tip);
  return pressed;
}

void tooltip(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", text);
  }
}

}  // namespace redgem::ui
