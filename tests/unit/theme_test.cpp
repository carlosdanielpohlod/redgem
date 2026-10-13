#include <doctest/doctest.h>

#include <imgui.h>

#include <initializer_list>

#include "editor/ui/style.hpp"

using namespace redgem;

namespace {

bool same(const ImVec4& a, const ImVec4& b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }

// An interface nobody draws: enough for a theme to be applied to.
struct Interface {
  Interface() { ImGui::CreateContext(); }
  ~Interface() { ImGui::DestroyContext(); }
};

}  // namespace

TEST_CASE("the theme marks what is checked, grabbed or selected with the accent color") {
  const Interface interface;
  ui::apply_theme();
  const ImGuiStyle& style = ImGui::GetStyle();
  for (const ImGuiCol slot : {ImGuiCol_CheckMark, ImGuiCol_SliderGrab, ImGuiCol_TabSelectedOverline}) {
    CHECK(same(style.Colors[slot], ui::accent_color()));
  }
  CHECK_FALSE(same(style.Colors[ImGuiCol_Text], style.Colors[ImGuiCol_WindowBg]));
  CHECK(style.Colors[ImGuiCol_WindowBg].w == 1.0f);
}

TEST_CASE("the theme rounds its windows and leaves docked panels without a menu button") {
  const Interface interface;
  ui::apply_theme();
  const ImGuiStyle& style = ImGui::GetStyle();
  CHECK(style.WindowRounding > 0.0f);
  CHECK(style.FrameBorderSize == 0.0f);
  CHECK(style.WindowMenuButtonPosition == ImGuiDir_None);
}

TEST_CASE("errors, warnings and successes each have a color of their own") {
  CHECK_FALSE(same(ui::error_color(), ui::warning_color()));
  CHECK_FALSE(same(ui::error_color(), ui::success_color()));
  CHECK_FALSE(same(ui::warning_color(), ui::success_color()));
  CHECK_FALSE(same(ui::muted_color(), ui::accent_color()));
}
