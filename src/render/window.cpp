#include "render/window.hpp"

#include <raylib.h>

#include <cstdarg>
#include <cstdio>
#include <stdexcept>

#include "core/log.hpp"
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
  unsigned int flags = FLAG_VSYNC_HINT;
  if (settings.resizable) flags |= FLAG_WINDOW_RESIZABLE;
  if (settings.smooth_edges) flags |= FLAG_MSAA_4X_HINT;
  return flags;
}

CursorChange cursor_change(bool captured, bool wanted) {
  if (captured == wanted) return CursorChange::None;
  return wanted ? CursorChange::Capture : CursorChange::Release;
}

Window::Window(const WindowSettings& settings) {
  prefer_discrete_gpu(settings.gpu);
  open_window(settings);
  SetExitKey(KEY_NULL);  // Esc belongs to what runs in the window: only its close button closes it
  SetTargetFPS(settings.frames_per_second);
}

Window::~Window() { CloseWindow(); }

bool Window::close_requested() const { return WindowShouldClose(); }

float Window::frame_seconds() const { return GetFrameTime(); }

SurfaceSize Window::drawing_size() const { return {GetRenderWidth(), GetRenderHeight()}; }

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
