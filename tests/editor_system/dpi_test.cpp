#include <doctest/doctest.h>
#include <imgui.h>
#include <raylib.h>

#include <cmath>
#include <filesystem>

#include "editor/editor_config.hpp"
#include "editor/scene_viewport.hpp"
#include "editor/ui/dpi.hpp"
#include "editor/ui/interface.hpp"
#include "editor/ui/style.hpp"
#include "window_mouse.hpp"
#include "window_picture.hpp"

using namespace redgem;
namespace {
constexpr int kWidth = 900;
constexpr int kHeight = 600;
constexpr int kSettleFrames = 4;
constexpr float kPanelWidth = 380;
constexpr float kPanelHeight = 240;
constexpr float kMargin = 20;
constexpr int kViewWidth = 160;
constexpr int kViewHeight = 72;
constexpr float kFov = 45;
constexpr float kPixelTolerance = 1;
WindowSettings settings() {
  WindowSettings result;
  result.width = kWidth;
  result.height = kHeight;
  result.title = "Editor display scale regression";
  result.high_dpi = ui::native_dpi_window();
  return result;
}
void draw_panel(SceneViewport& view) {
  ImGui::SetNextWindowPos(ImVec2(ui::scaled(kMargin), ui::scaled(kMargin)));
  ImGui::SetNextWindowSize(ImVec2(ui::scaled(kPanelWidth), ui::scaled(kPanelHeight)));
  ImGui::Begin("Display scale");
  ImGui::TextUnformatted(ICON_FA_GEM " Interface text and icons");
  CHECK(ImGui::GetFontSize() == doctest::Approx(std::round(editor_config::kInterfaceFontSize * ui::scaled(1))));
  ImGui::PushFont(ui::code_font());
  ImGui::TextUnformatted("object.position = Vec3.new(1, 2, 3)");
  CHECK(ImGui::GetFontSize() == doctest::Approx(std::round(editor_config::kCodeFontSize * ui::scaled(1))));
  ImGui::PopFont();
  ImGui::Button("A scaled button");
  const auto origin = ImGui::GetCursorScreenPos();
  view.screen_x = origin.x;
  view.screen_y = origin.y;
  ImGui::Image(ImTextureRef(static_cast<ImTextureID>(view.target.texture.id)),
               ImVec2(view.width_pixels(), view.height_pixels()));
  ImGui::End();
}
void frame(Window& window, ui::Interface& interface, SceneViewport& view) {
  window.begin_frame();
  ClearBackground(::Color{25, 25, 30, 255});
  interface.begin_frame();
  draw_panel(view);
  interface.end_frame();
  window.end_frame();
}
void check_pointer(SceneViewport& view) {
  GatedInputSource input;
  input.enabled = true;
  input.origin_x = view.screen_x;
  input.origin_y = view.screen_y;
  const auto mouse = input.mouse();
  CHECK(std::abs(mouse.x - view.width_pixels() * kHalf) <= kPixelTolerance);
  CHECK(std::abs(mouse.y - view.height_pixels() * kHalf) <= kPixelTolerance);
  const auto ray = view.ray_at(mouse.x, mouse.y);
  CHECK(ray.direction.z < -0.99f);
}
}  // namespace
TEST_CASE("interface scale changes keep fonts alive and viewport mouse coordinates aligned") {
  Window window(settings());
  float requested_scale = 1;
  ui::Interface interface(window, ui::Interface::Layout::Fixed, [&] { return requested_scale; });
  SceneViewport view;
  view.camera = {{0, 0, 5}, {0, 0, 0}, {0, 1, 0}, kFov, CAMERA_PERSPECTIVE};
  auto* text = ImGui::GetIO().Fonts->Fonts[0];
  auto* code = ui::code_font();
  for (const float scale : {1.0f, 1.5f, 2.0f, 1.0f}) {
    requested_scale = scale;
    view.resize(static_cast<int>(kViewWidth * scale), static_cast<int>(kViewHeight * scale));
    BeginTextureMode(view.target);
    ClearBackground(::Color{55, 110, 160, 255});
    EndTextureMode();
    for (int index = 0; index < kSettleFrames; ++index) frame(window, interface, view);
    test::move_window_mouse(view.screen_x + view.width_pixels() * kHalf, view.screen_y + view.height_pixels() * kHalf);
    for (int index = 0; index < kSettleFrames; ++index) frame(window, interface, view);
    check_pointer(view);
    CHECK(ImGui::GetIO().Fonts->Fonts[0] == text);
    CHECK(ui::code_font() == code);
    const auto name = "dpi_" + std::to_string(static_cast<int>(scale * 100)) + ".png";
    CHECK(test::save_window_picture(std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor" / name));
  }
}
