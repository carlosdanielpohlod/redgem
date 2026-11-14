// System test: one process opens and closes window after window, as the render system tests do,
// and every window opens and draws. Each window used to load the OpenGL library and Mesa's
// driver again, and after about a hundred of them the driver no longer loaded (see
// keep_window_libraries_loaded in render/window.cpp). Linux only: the limit is glibc's.
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

void draw_a_frame(Window& window) {
  window.begin_frame();
  ClearBackground(BLACK);
  window.end_frame();
}

// Opens a window, draws one frame in it and closes it (a window that does not open throws).
void open_draw_and_close() {
  Window window(small_window());
  draw_a_frame(window);
}

}  // namespace

TEST_CASE("a process opens, draws in and closes 300 windows one after another") {
  for (int window = 0; window < kWindows; ++window) {
    INFO("window " << window + 1 << " of " << kWindows);  // reported on a throw or a crash too
    REQUIRE_NOTHROW(open_draw_and_close());
  }
  CHECK_FALSE(IsWindowReady());  // the last one closed too
}
