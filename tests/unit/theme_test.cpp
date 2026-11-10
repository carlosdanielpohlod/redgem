#include <doctest/doctest.h>

#include <imgui.h>

#include <initializer_list>
#include <cmath>
#include <limits>

#include "editor/ui/style.hpp"
#include "editor/ui/dpi.hpp"

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

TEST_CASE("monitor DPI is scaled once in pixel coordinates and kept logical on Retina") {
  constexpr float kFractional = 1.5f;
  constexpr float kDouble = 2;
  for (const float scale : {1.0f, kFractional, kDouble}) {
    CHECK(ui::interface_scale(scale, false) == scale);
    CHECK(ui::interface_scale(scale, true) == 1);
  }
  CHECK(ui::interface_scale(std::numeric_limits<float>::quiet_NaN(), false) == 1);
  CHECK(ui::interface_scale(std::numeric_limits<float>::infinity(), false) == 1);
  CHECK(ui::interface_scale(0, false) == 1);
  CHECK(ui::interface_scale(-1, false) == 1);
}
TEST_CASE("monitor changes rebuild spacing without accumulating scaling or changing the palette") {
  const Interface interface;
  ui::apply_interface_scale(1);
  const auto baseline = ImGui::GetStyle();
  for (const float scale : {1.5f, 2.0f, 1.5f, 1.0f}) {
    ui::apply_interface_scale(scale);
    const auto& style = ImGui::GetStyle();
    CHECK(style.FontScaleDpi == scale);
    CHECK(style.WindowPadding.x == doctest::Approx(baseline.WindowPadding.x * scale));
    CHECK(style.FramePadding.y == doctest::Approx(std::floor(baseline.FramePadding.y * scale)));
    CHECK(style.ScrollbarSize == doctest::Approx(baseline.ScrollbarSize * scale));
    CHECK(same(style.Colors[ImGuiCol_Button], baseline.Colors[ImGuiCol_Button]));
  }
}
