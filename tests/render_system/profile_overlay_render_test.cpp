#include <doctest/doctest.h>
#include <raylib.h>
#include <filesystem>

#include "app/engine.hpp"
#include "app/window_presenter.hpp"
#include "core/config.hpp"
#include "test_support.hpp"
#include "async_scene_support.hpp"

using namespace redgem;
namespace {
constexpr SurfaceSize kSurface{640, 360};
constexpr int kOutsidePanelX = 500;
constexpr int kInsidePanel = 4;
struct Pixels {
  ::Color inside{};
  ::Color outside{};
};
void ruby(Engine& engine, const char* text) { REQUIRE(engine.scripts().eval(text, "profile_window.rb").ok); }
}  // namespace
TEST_CASE("profile overlay draws above the game and disappears when disabled") {
  Pixels pixels;
  const char* capture = nullptr;
  GameWindowOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.capture_cursor = false;
  DisplaySettings display;
  display.resolution = kSurface;
  display.quality = QualityPreset::Low;
  display.vsync = false;
  display.fps_cap = 0;
  SUBCASE("native render size") {}
  SUBCASE("scaled world with native HUD") { display.render_scale = 0.5f; }
  const auto mode = display.render_scale < 1.0f ? "scaled" : "native";
  const auto folder = std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "profiling" / mode;
  std::filesystem::create_directories(folder);
  options.display = display;
  options.show_fps = true;
  options.on_frame_drawn = [&] {
    Image image = LoadImageFromScreen();
    REQUIRE(image.data);
    pixels.inside = GetImageColor(image, kInsidePanel, kInsidePanel);
    pixels.outside = GetImageColor(image, kOutsidePanelX, kInsidePanel);
    if (capture) ExportImage(image, (folder / capture).string().c_str());
    UnloadImage(image);
  };
  WindowPresenter window(options);
  test::HeldSceneReader reader;
  EngineOptions run;
  run.game_dir = options.game_dir;
  run.presenter = &window;
  run.scene_loader = &reader.loader;
  run.display = display;
  auto created = Engine::create(run);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  auto& engine = *std::get<std::unique_ptr<Engine>>(created);
  ruby(engine, "Game.on_loading { UI.rect(x: 0, y: 0, width: UI.width, height: UI.height, color: :white) }");
  ruby(engine, "Debug.stats_visible = true; Game.load_scene_async('level2')");
  engine.frame(config::kFixedDeltaSeconds);
  reader.wait_started();
  engine.frame(config::kFixedDeltaSeconds);  // collect a completion interval before capturing FPS
  capture = "enabled.png";
  engine.frame(config::kFixedDeltaSeconds);
  CHECK(pixels.inside.r < pixels.outside.r);
  ruby(engine, "Debug.stats_visible = false");
  capture = "disabled.png";
  engine.frame(config::kFixedDeltaSeconds);
  CHECK_FALSE(engine.script_context().stats_visible);
  CHECK(pixels.inside.r == pixels.outside.r);
}
