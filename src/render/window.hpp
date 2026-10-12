#pragma once

#include <string>

#include "render/gpu_select.hpp"
#include "render/render_config.hpp"

namespace redgem {

// What a window is opened with.
struct WindowSettings {
  int width = 0;
  int height = 0;
  std::string title;
  bool resizable = false;
  // Smooths the edges of what is drawn straight into the window (an interface). A game needs
  // none: its picture is drawn off screen and smoothed there.
  bool smooth_edges = false;
  int frames_per_second = config::kTargetFps;  // the pace frames are held to
  GpuChoice gpu = GpuChoice::Auto;             // the card asked for, before the window opens
};

// Width and height of something drawn on, in pixels.
struct SurfaceSize {
  int width = 0;
  int height = 0;
};

// The flags the window library is given before it opens a window with these settings.
unsigned int window_flags(const WindowSettings& settings);
// What a pointer that is captured or not (`captured`) has to become when it is wanted `wanted`.
enum class CursorChange { None, Capture, Release };
CursorChange cursor_change(bool captured, bool wanted);

// The system's window, and the only thing that opens and closes one: the card it draws with, its
// size and title, the pace of its frames, its pointer. One at a time; whatever draws in it (a
// renderer, an interface) is made after it and released before it.
class Window {
 public:
  // Throws std::runtime_error when the system gives no window.
  explicit Window(const WindowSettings& settings);
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  // Whether the person asked for it to close (true for one frame only).
  bool close_requested() const;
  // How long the frame before this one took.
  float frame_seconds() const;
  // Size of the area that is drawn in.
  SurfaceSize drawing_size() const;
  void set_title(const std::string& title);
  // Hides the pointer and keeps it in the window (first-person cameras), or gives it back.
  void set_cursor_locked(bool locked);
  // Everything drawn to the window in a frame goes between these two; the second shows it.
  void begin_frame();
  void end_frame();
  // The frames-per-second counter, in its corner: for whoever makes the game.
  void draw_frame_rate();
};

}  // namespace redgem
