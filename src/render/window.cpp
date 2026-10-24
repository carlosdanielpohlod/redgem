#include "render/window.hpp"

#include <raylib.h>

#include <cstdarg>
#include <cstdio>
#include <stdexcept>

#include "core/log.hpp"
#include "render/gl_extras.hpp"
#include "render/render_math.hpp"

namespace redgem {
namespace {

constexpr int kTraceLineLength = 512;  // characters of a raylib log line that are kept

// Passes on raylib's warnings and the one line that says which graphics card draws the window.
void report_graphics_card(int level, const char* format, va_list args) {
  char line[kTraceLineLength];
  std::vsnprintf(line, sizeof(line), format, args);
  const std::string text = line;
  if (const auto card = render_math::graphics_card_name(text)) {
    log::info("graphics card: " + *card);
  } else if (level >= LOG_WARNING) {
    log::warning(text);
  }
}

// Opens the window. raylib names the graphics card only at its most verbose level: listen while
// it opens.
void open_window(const WindowSettings& settings) {
  SetTraceLogCallback(report_graphics_card);
  SetTraceLogLevel(LOG_INFO);
  SetConfigFlags(window_flags(settings));
  InitWindow(settings.width, settings.height, settings.title.c_str());
  SetTraceLogCallback(nullptr);
  SetTraceLogLevel(LOG_WARNING);
  if (!IsWindowReady()) throw std::runtime_error("cannot open a window");
}

}  // namespace

unsigned int window_flags(const WindowSettings& settings) {
  unsigned int flags = settings.vsync ? FLAG_VSYNC_HINT : 0U;
  if (settings.resizable) flags |= FLAG_WINDOW_RESIZABLE;
  if (settings.smooth_edges) flags |= FLAG_MSAA_4X_HINT;
  return flags;
}

namespace {

// Whether a window going to `wanted_mode` is given another size: a borderless one is the monitor's.
bool resized(WindowMode wanted_mode, SurfaceSize size, SurfaceSize wanted_size) {
  return wanted_mode != WindowMode::Borderless && wanted_size != size;
}

// Whether a window that changes (`reshaped`) goes through the mode it is in, or to the one wanted:
// a window is the one mode not gone into or out of.
bool through_mode(bool reshaped, WindowMode mode) { return reshaped && mode != WindowMode::Windowed; }

}  // namespace

WindowChange window_change(WindowMode mode, SurfaceSize size, WindowMode wanted_mode, SurfaceSize wanted_size) {
  WindowChange change;
  change.resize = resized(wanted_mode, size, wanted_size);
  const bool reshaped = change.resize || wanted_mode != mode;
  change.leave_mode = through_mode(reshaped, mode);
  change.enter_mode = through_mode(reshaped, wanted_mode);
  return change;
}

SurfaceSize kept_size(WindowMode mode, SurfaceSize asked, SurfaceSize kept) {
  return mode == WindowMode::Borderless ? kept : asked;
}

CursorChange cursor_change(bool captured, bool wanted) {
  if (captured == wanted) return CursorChange::None;
  return wanted ? CursorChange::Capture : CursorChange::Release;
}

Window::Window(const WindowSettings& settings) : size_{settings.width, settings.height}, vsync_(settings.vsync) {
  prefer_discrete_gpu(settings.gpu);
  open_window(settings);
  SetExitKey(KEY_NULL);  // Esc belongs to what runs in the window: only its close button closes it
  SetTargetFPS(settings.frames_per_second);
  reshape(settings.mode, size_);
}

Window::~Window() { CloseWindow(); }

bool Window::close_requested() const { return WindowShouldClose(); }

float Window::frame_seconds() const { return GetFrameTime(); }

SurfaceSize Window::drawing_size() const { return {GetRenderWidth(), GetRenderHeight()}; }

void Window::pace(const DisplaySettings& display) {
  set_vsync(display.vsync);
  SetTargetFPS(held_fps_cap(display.fps_cap));
}

SurfaceSize Window::monitor_size() const { return monitor_; }

int Window::largest_picture_side() const { return render_detail::largest_target_side(); }

// (Read while the window is a window: in fullscreen, the monitor says the size it was switched to.)
void Window::follow_monitor() {
  if (mode_ != WindowMode::Windowed) return;
  const int monitor = GetCurrentMonitor();
  monitor_ = {GetMonitorWidth(monitor), GetMonitorHeight(monitor)};
}

void Window::reshape(WindowMode mode, SurfaceSize size) {
  follow_monitor();
  const WindowChange change = window_change(mode_, size_, mode, size);
  if (change.leave_mode) toggle(mode_);
  if (change.resize) SetWindowSize(size.width, size.height);
  if (change.enter_mode) toggle(mode);
  mode_ = mode;
  size_ = kept_size(mode, size, size_);
}

// Goes into a mode from a window, or back to a window from it: raylib toggles each one.
void Window::toggle(WindowMode mode) {
  if (mode == WindowMode::Fullscreen) ToggleFullscreen();
  if (mode == WindowMode::Borderless) ToggleBorderlessWindowed();
}

void Window::set_vsync(bool on) {
  if (on == vsync_) return;
  vsync_ = on;
  if (on) {
    SetWindowState(FLAG_VSYNC_HINT);
  } else {
    ClearWindowState(FLAG_VSYNC_HINT);
  }
}

void Window::set_title(const std::string& title) { SetWindowTitle(title.c_str()); }

void Window::set_cursor_locked(bool locked) {
  const CursorChange change = cursor_change(IsCursorHidden(), locked);
  if (change == CursorChange::Capture) DisableCursor();
  if (change == CursorChange::Release) EnableCursor();
}

void Window::begin_frame() { BeginDrawing(); }

void Window::end_frame() { EndDrawing(); }

void Window::draw_frame_rate() { DrawFPS(config::kFpsCounterX, config::kFpsCounterY); }

}  // namespace redgem
