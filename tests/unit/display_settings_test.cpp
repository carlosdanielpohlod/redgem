// The player's display settings: their names, what they are held to, how they are kept, and the
// sizes the world is drawn at.
#include <doctest/doctest.h>

#include <limits>

#include "render/display_settings.hpp"

using namespace redgem;

TEST_CASE("a player who chose nothing gets a window at the engine's pace, with every quality a level asks for") {
  const DisplaySettings display;
  CHECK(display.mode == WindowMode::Windowed);
  CHECK(display.vsync);
  CHECK(display.fps_cap == config::kTargetFps);
  CHECK(display.render_scale == kFullRenderScale);
  CHECK(display.quality == QualityPreset::Ultra);
  CHECK(display.volume == doctest::Approx(1.0f));
  CHECK(quality_caps(display) == QualityCaps{});
}

TEST_CASE("window modes and qualities go by their names, and nothing else is one") {
  for (const WindowMode mode : {WindowMode::Windowed, WindowMode::Fullscreen, WindowMode::Borderless}) {
    CHECK(window_mode_from_name(window_mode_name(mode)) == mode);
  }
  for (const QualityPreset quality :
       {QualityPreset::Low, QualityPreset::Medium, QualityPreset::High, QualityPreset::Ultra, QualityPreset::Custom}) {
    CHECK(quality_from_name(quality_name(quality)) == quality);
  }
  CHECK(std::string(window_mode_name(WindowMode::Borderless)) == "borderless");
  CHECK_FALSE(window_mode_from_name("maximized"));
  CHECK_FALSE(quality_from_name("Ultra"));
}

TEST_CASE("the render scale is held from half the window to twice it") {
  CHECK(held_render_scale(0.1f) == kSmallestRenderScale);
  CHECK(held_render_scale(3.0f) == kLargestRenderScale);
  CHECK(held_render_scale(0.75f) == doctest::Approx(0.75f));
  CHECK(held_render_scale(std::numeric_limits<float>::quiet_NaN()) == kFullRenderScale);
  CHECK_FALSE(scales_picture(1.0f));
  CHECK(scales_picture(0.5f));
  CHECK(scales_picture(1.25f));
}

TEST_CASE("the world is drawn at the window's size times the render scale, at least a pixel a side") {
  CHECK(scaled_size({1280, 720}, 0.5f) == SurfaceSize{640, 360});
  CHECK(scaled_size({1280, 720}, 2.0f) == SurfaceSize{2560, 1440});
  CHECK(scaled_size({1366, 768}, 0.75f) == SurfaceSize{1025, 576});  // (rounded to the nearest pixel)
  CHECK(scaled_size({1280, 720}, 1.0f) == SurfaceSize{1280, 720});
  CHECK(scaled_size({1, 1}, 0.5f) == SurfaceSize{1, 1});
  CHECK(scaled_size({800, 600}, 9.0f) == SurfaceSize{1600, 1200});  // held to twice
}

TEST_CASE("a frame cap is none or a pace a monitor can show") {
  CHECK(held_fps_cap(-5) == kNoFrameCap);
  CHECK(held_fps_cap(144) == 144);
  CHECK(held_fps_cap(100000) == kMostFramesPerSecond);
}

TEST_CASE("the settings are kept as they are read back") {
  DisplaySettings display;
  display.mode = WindowMode::Fullscreen;
  display.resolution = {1920, 1080};
  display.render_scale = 0.75f;
  display.vsync = false;
  display.fps_cap = kNoFrameCap;
  display.quality = QualityPreset::Custom;
  display.custom_quality = preset_caps(QualityPreset::Medium);
  display.volume = 0.25f;
  const DisplaySettings read = display_from_json(display_json(display), DisplaySettings{});
  CHECK(read.mode == WindowMode::Fullscreen);
  CHECK(read.resolution == SurfaceSize{1920, 1080});
  CHECK(read.render_scale == doctest::Approx(0.75f));
  CHECK_FALSE(read.vsync);
  CHECK(read.fps_cap == kNoFrameCap);
  CHECK(read.quality == QualityPreset::Custom);
  CHECK(read.custom_quality == preset_caps(QualityPreset::Medium));
  CHECK(display_from_json({{"custom_quality", {{"msaa", 2.0}, {"shadow_cascades", 2.4}}}}, read).custom_quality.msaa ==
        2);
  CHECK(read.volume == doctest::Approx(0.25f));
}

TEST_CASE("what is missing or not what it should be stays as it was, and what is out of range is held to it") {
  DisplaySettings base;
  base.resolution = {1280, 720};
  const nlohmann::json json = {{"window_mode", "maximized"},
                               {"width", 10},
                               {"height", "tall"},
                               {"render_scale", 7.5},
                               {"vsync", "yes"},
                               {"fps_cap", -1},
                               {"quality", 3},
                               {"volume", 4.0},
                               {"custom_quality", {{"msaa", 64}}}};
  const DisplaySettings read = display_from_json(json, base);
  CHECK(read.mode == WindowMode::Windowed);
  CHECK(read.resolution == SurfaceSize{config::kMinWindowSide, 720});
  CHECK(read.render_scale == kLargestRenderScale);
  CHECK(read.vsync);
  CHECK(read.fps_cap == kNoFrameCap);
  CHECK(read.quality == QualityPreset::Ultra);
  CHECK(read.volume == doctest::Approx(1.0f));
  CHECK(read.custom_quality.msaa == config::kMostSamples);
  CHECK(display_from_json(nlohmann::json::array(), base).resolution == base.resolution);
}

TEST_CASE("a custom quality changed in part keeps the rest of what it was") {
  DisplaySettings base;
  base.custom_quality.bloom = 0.5f;
  const DisplaySettings read =
      display_from_json({{"quality", "custom"}, {"custom_quality", {{"antialias", false}}}}, base);
  CHECK(read.quality == QualityPreset::Custom);
  CHECK_FALSE(quality_caps(read).temporal_antialias);
  CHECK(quality_caps(read).bloom == doctest::Approx(0.5f));
}

TEST_CASE("the window sizes offered are the usual ones that fit the monitor, and the monitor's own") {
  const std::vector<SurfaceSize> sizes = window_sizes_for({1920, 1080});
  REQUIRE_FALSE(sizes.empty());
  CHECK(sizes.front() == SurfaceSize{640, 360});
  CHECK(sizes.back() == SurfaceSize{1920, 1080});
  for (const SurfaceSize size : sizes) CHECK(size.width <= 1920);
  CHECK(window_sizes_for({1000, 700}).back() == SurfaceSize{1000, 700});
  CHECK(window_sizes_for({0, 0}).size() > 1);  // a monitor of no known size: every usual one
}
