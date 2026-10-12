// The parts of opening a window that need no window.
#include <doctest/doctest.h>

#include <raylib.h>

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
  const WindowSettings window = game_window_settings(game, options);
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
  const WindowSettings window = game_window_settings(game, options);
  CHECK(window.height == 720);
  CHECK(window.width == 1440);
}

TEST_CASE("a game's window gives the game the pointer and shows no frame counter unless told") {
  const GameWindowOptions options;
  CHECK(options.capture_cursor);
  CHECK_FALSE(options.show_fps);
  CHECK(options.screen_aspect == doctest::Approx(0.0f));
}
