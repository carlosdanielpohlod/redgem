// The parts of opening a window that need no window.
#include <doctest/doctest.h>

#include <raylib.h>

#include <array>

#include "app/window_presenter.hpp"
#include "render/render_config.hpp"
#include "render/window.hpp"

using namespace redgem;

TEST_CASE("a window is asked of the system with frames in step with the screen, and what its settings add") {
  WindowSettings settings;
  CHECK(window_flags(settings) == FLAG_VSYNC_HINT);
  settings.resizable = true;
  CHECK(window_flags(settings) == (FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE));
  settings.smooth_edges = true;
  CHECK(window_flags(settings) == (FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT));
}

TEST_CASE("a window holds the engine's pace and asks for the dedicated card unless told otherwise") {
  const WindowSettings settings;
  CHECK(settings.frames_per_second == config::kTargetFps);
  CHECK(settings.gpu == GpuChoice::Auto);
  CHECK_FALSE(settings.resizable);
}

TEST_CASE("the pointer is only captured or given back when that changes something") {
  CHECK(cursor_change(false, false) == CursorChange::None);
  CHECK(cursor_change(true, true) == CursorChange::None);
  CHECK(cursor_change(false, true) == CursorChange::Capture);
  CHECK(cursor_change(true, false) == CursorChange::Release);
}

TEST_CASE("a game's window is the one its settings ask for, on the card its player chose") {
  GameSettings game;
  game.title = "Demo";
  game.window_width = 800;
  game.window_height = 600;
  GameWindowOptions options;
  options.gpu = GpuChoice::Integrated;
  DisplaySettings display;
  display.resolution = {800, 600};
  const WindowSettings window = game_window_settings(game, display, options);
  CHECK(window.title == "Demo");
  CHECK(window.width == 800);
  CHECK(window.height == 600);
  CHECK(window.gpu == GpuChoice::Integrated);
  CHECK_FALSE(window.resizable);
  CHECK_FALSE(window.smooth_edges);  // the game's picture is smoothed off screen
}

TEST_CASE("on a screen with a shape of its own the game keeps its height and gets the width that fills it") {
  GameSettings game;
  game.window_width = 1280;
  game.window_height = 720;
  GameWindowOptions options;
  options.screen_aspect = 2.0f;  // a phone held sideways
  DisplaySettings display;
  display.resolution = {640, 480};
  display.mode = WindowMode::Fullscreen;
  const WindowSettings window = game_window_settings(game, display, options);
  CHECK(window.height == 720);
  CHECK(window.width == 1440);
  CHECK(window.mode == WindowMode::Windowed);  // (a phone's screen has one mode)
}

TEST_CASE("a game's window opens as its player chose: its size, its mode, its vsync and its frame cap") {
  DisplaySettings display;
  display.resolution = {1920, 1080};
  display.mode = WindowMode::Borderless;
  display.vsync = false;
  display.fps_cap = kNoFrameCap;
  const WindowSettings window = game_window_settings(GameSettings{}, display, GameWindowOptions{});
  CHECK(window.width == 1920);
  CHECK(window.height == 1080);
  CHECK(window.mode == WindowMode::Borderless);
  CHECK_FALSE(window.vsync);
  CHECK(window.frames_per_second == kNoFrameCap);
  CHECK(window_flags(window) == 0U);  // no vsync asked of the system
}

TEST_CASE("a window changes only what has to change to go from one mode and size to another") {
  const SurfaceSize small{800, 600};
  const SurfaceSize large{1920, 1080};
  const auto change = [](WindowMode mode, SurfaceSize size, WindowMode wanted, SurfaceSize wanted_size) {
    const WindowChange made = window_change(mode, size, wanted, wanted_size);
    return std::array<bool, 3>{made.leave_mode, made.resize, made.enter_mode};
  };
  using M = WindowMode;
  CHECK(change(M::Windowed, small, M::Windowed, small) == std::array<bool, 3>{false, false, false});
  CHECK(change(M::Windowed, small, M::Windowed, large) == std::array<bool, 3>{false, true, false});
  CHECK(change(M::Windowed, small, M::Fullscreen, small) == std::array<bool, 3>{false, false, true});
  CHECK(change(M::Windowed, small, M::Fullscreen, large) == std::array<bool, 3>{false, true, true});
  // Out of fullscreen to change its size, and back in.
  CHECK(change(M::Fullscreen, small, M::Fullscreen, large) == std::array<bool, 3>{true, true, true});
  CHECK(change(M::Fullscreen, small, M::Windowed, small) == std::array<bool, 3>{true, false, false});
  CHECK(change(M::Fullscreen, small, M::Borderless, small) == std::array<bool, 3>{true, false, true});
  // A borderless window is as big as its monitor, whatever size is asked.
  CHECK(change(M::Borderless, small, M::Borderless, large) == std::array<bool, 3>{false, false, false});
  CHECK(change(M::Borderless, small, M::Windowed, large) == std::array<bool, 3>{true, true, false});
}

TEST_CASE("a game's window gives the game the pointer and shows no frame counter unless told") {
  const GameWindowOptions options;
  CHECK(options.capture_cursor);
  CHECK_FALSE(options.show_fps);
  CHECK(options.screen_aspect == doctest::Approx(0.0f));
}
