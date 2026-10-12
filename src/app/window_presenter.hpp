#pragma once

#include <filesystem>
#include <functional>

#include "app/frame_painter.hpp"
#include "app/game_settings.hpp"
#include "app/presenter.hpp"
#include "input/raylib_input_source.hpp"
#include "render/gpu_select.hpp"
#include "render/renderer.hpp"
#include "render/window.hpp"

namespace redgem {

// The window a game is shown in, as whoever plays it decides.
struct GameWindowOptions {
  // The game: its game.json says the window's title and size, its images/ folder holds its pictures.
  std::filesystem::path game_dir;
  // The graphics card the window is opened on, where the machine has two.
  GpuChoice gpu = GpuChoice::Auto;
  // The frames-per-second counter in the corner of the window: for whoever makes the game, never
  // for who plays it.
  bool show_fps = false;
  // Whether the game may hide the pointer and keep it in the window (Input.mouse_locked). Off, the
  // pointer stays with the person at the machine.
  bool capture_cursor = true;
  // Width over height of the screen the game fills (a phone's); 0 for a window sized by the
  // game's settings.
  float screen_aspect = 0.0f;
  // Called once each frame is drawn and before it is shown, for whoever wants its picture.
  std::function<void()> on_frame_drawn;
};

// The window those options and a game's settings ask for. A screen that is not a window has its
// own shape: the game keeps its height and gets the width that fills it.
WindowSettings game_window_settings(const GameSettings& settings, const GameWindowOptions& options);

// Shows a game in a window of its own: opens the window, draws each frame in it, and reads the
// keyboard, mouse and gamepad that come with it.
class WindowPresenter final : public Presenter {
 public:
  // Throws std::runtime_error when no window can be opened.
  explicit WindowPresenter(GameWindowOptions options);

  SurfaceSize surface_size() const override { return window_.drawing_size(); }
  int text_width(const std::string& text, int size) const override { return renderer_.text_width(text, size); }
  void capture_cursor(bool wanted) override;
  void present(const FrameView& view) override;
  bool open() const override { return !window_.close_requested(); }
  float frame_seconds() const override { return window_.frame_seconds(); }

  // The keyboard, mouse and gamepad of the window.
  InputSource& devices() { return devices_; }

 private:
  GameWindowOptions options_;
  Window window_;
  Renderer renderer_;  // (made after the window, released before it)
  FramePainter painter_;
  RaylibInputSource devices_;
};

// Plays a game in a window of its own, with the machine's devices and sound, until the window
// is closed or the game quits. Returns the process exit code.
int play_in_window(const GameWindowOptions& options);
// The same, with the game reading `input` instead of the window's own devices: for a program
// whose screen brings more than a keyboard, a mouse and a gamepad (a touch screen).
int play_in_window(const GameWindowOptions& options, InputSource& input);

}  // namespace redgem
