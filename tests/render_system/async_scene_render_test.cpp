#include <doctest/doctest.h>
#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "app/engine.hpp"
#include "app/window_presenter.hpp"
#include "async_scene_support.hpp"
#include "core/config.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {
constexpr SurfaceSize kLoadingWindow{640, 360};
constexpr int kPicturePoint = 8;  // inside the loading screen's opaque red rectangle
constexpr int kFramesWhileBlocked = 5;
constexpr int kRedDominance = 3;

void ruby(Engine& engine, const char* source) {
  const auto result = engine.scripts().eval(source, "loading_window.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
}
std::filesystem::path picture(const char* name) {
  const auto path = std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "loading" / name;
  std::filesystem::create_directories(path.parent_path());
  return path;
}
}  // namespace

TEST_CASE("async loading draws a live loading screen in the player's window") {
  const auto game = test::fixture("games/sandbox");
  GameWindowOptions window_options;
  window_options.game_dir = game;
  window_options.capture_cursor = false;
  DisplaySettings display;
  display.resolution = kLoadingWindow;
  display.quality = QualityPreset::Low;
  display.vsync = false;
  display.fps_cap = 0;
  window_options.display = display;
  const char* capture = nullptr;
  ::Color seen{};
  window_options.on_frame_drawn = [&] {
    if (!capture) return;
    Image image = LoadImageFromScreen();
    REQUIRE(image.data);
    seen = GetImageColor(image, kPicturePoint, kPicturePoint);
    CHECK(ExportImage(image, picture(capture).string().c_str()));
    UnloadImage(image);
  };
  WindowPresenter window(window_options);
  test::HeldSceneReader reader;
  EngineOptions options;
  options.game_dir = game;
  options.presenter = &window;
  options.display = display;
  options.scene_loader = &reader.loader;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  auto& engine = *std::get<std::unique_ptr<Engine>>(created);
  ruby(engine,
       "Game.on_loading { |dt| UI.rect(x: 0, y: 0, width: UI.width, height: UI.height, color: :ruby); "
       "UI.text('Loading level2...', x: 32, y: 80, size: 32) }; Game.load_scene_async('level2')");
  engine.frame(config::kFixedDeltaSeconds);
  reader.wait_started();
  std::vector<double> milliseconds;
  capture = "reading.png";
  for (int i = 0; i < kFramesWhileBlocked; ++i) {
    const auto start = std::chrono::steady_clock::now();
    engine.frame(config::kFixedDeltaSeconds);
    milliseconds.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    capture = nullptr;
  }
  CHECK(seen.r > kRedDominance * seen.g);
  CHECK(engine.scene_name() == "main");
  reader.release();
  const auto deadline = std::chrono::steady_clock::now() + test::kSceneWaitLimit;
  double activation_frame = 0.0;
  while (engine.scene_name() != "level2" && std::chrono::steady_clock::now() < deadline) {
    const auto frame_start = std::chrono::steady_clock::now();
    engine.frame(config::kFixedDeltaSeconds);
    activation_frame =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frame_start).count();
  }
  REQUIRE(engine.scene_name() == "level2");
  capture = "activated.png";
  const auto start = std::chrono::steady_clock::now();
  engine.frame(config::kFixedDeltaSeconds);
  const auto activation = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  CHECK_FALSE(seen.r > kRedDominance * seen.g);
  std::ofstream timings(picture("frame-times.txt"));
  for (const auto ms : milliseconds) timings << "loading_ms=" << ms << '\n';
  timings << "activation_frame_ms=" << activation_frame << '\n';
  timings << "first_new_scene_render_ms=" << activation << '\n';
  MESSAGE("Loading max ms: " << *std::max_element(milliseconds.begin(), milliseconds.end()) << "; activation frame ms: "
                             << activation_frame << "; first new render ms: " << activation);
}
