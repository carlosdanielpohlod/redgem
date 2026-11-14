// System test: one process opens and closes window after window, as the render system tests do,
// and every window opens and draws. Each window used to load the OpenGL library and Mesa's
// driver again, and after about a hundred of them the driver no longer loaded (see
// keep_libraries_loaded in render/window.cpp).
#include <doctest/doctest.h>

#include <raylib.h>

#include "render/window.hpp"

using namespace redgem;

namespace {

constexpr int kWindows = 300;  // well past where the driver used to stop loading
constexpr int kSide = 64;      // pixels across the window, and down
constexpr const char* kTitle = "RedGem window reopen test";

WindowSettings small_window() {
  WindowSettings settings{kSide, kSide, kTitle};
  settings.vsync = false;  // nothing here waits for a monitor
  return settings;
}

// Opens a window, draws one frame in it and closes it; whether it opened.
bool open_draw_and_close() {
  Window window(small_window());
  window.begin_frame();
  ClearBackground(BLACK);
  window.end_frame();
  return IsWindowReady();
}

}  // namespace

TEST_CASE("a process opens, draws in and closes 300 windows one after another") {
  int opened = 0;
  for (int window = 0; window < kWindows; ++window) {
    REQUIRE_MESSAGE(open_draw_and_close(), "window " << window + 1 << " of " << kWindows);
    ++opened;
  }
  CHECK(opened == kWindows);
  CHECK_FALSE(IsWindowReady());  // the last one closed too
}
