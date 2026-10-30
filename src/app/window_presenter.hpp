#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "app/frame_painter.hpp"
#include "app/game_display.hpp"
#include "app/game_settings.hpp"
#include "app/presenter.hpp"
#include "app/recording.hpp"
#include "app/video_recorder.hpp"
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
  // How the game is shown when the window opens: what its game.json says when none.
  std::optional<DisplaySettings> display;
  // Where the player's choices are kept for the next runs (see EngineOptions::display_kept_in):
  // none for every run but a player's (see keeps_display).
  std::optional<DisplayKeeping> display_kept_in;
  // Records the game to a video (see recording.hpp): drawn off screen at the size asked for, each
  // frame advancing the game by exactly 1/fps, the window showing what is recorded.
  std::optional<RecordOptions> record;
};

// The window those options, a game's settings and its player's display settings ask for. A screen
// that is not a window has its own shape: the game keeps its height and gets the width that fills
// it, in the one mode it has.
WindowSettings game_window_settings(const GameSettings& settings, const DisplaySettings& display,
                                    const GameWindowOptions& options);

// Shows a game in a window of its own: opens the window, draws each frame in it, and reads the
// keyboard, mouse and gamepad that come with it.
class WindowPresenter final : public Presenter {
 public:
  // Throws std::runtime_error when no window can be opened.
  explicit WindowPresenter(GameWindowOptions options);
  ~WindowPresenter() override;
  WindowPresenter(const WindowPresenter&) = delete;
  WindowPresenter& operator=(const WindowPresenter&) = delete;

  SurfaceSize surface_size() const override;
  SurfaceSize picture_size() const override;
  int text_width(const std::string& text, int size) const override { return renderer_.text_width(text, size); }
  int font_text_width(const std::string& text, int size, const std::string& font,
                      const std::filesystem::path& folder) const override {
    return renderer_.font_text_width(text, size, font, folder);
  }
  void capture_cursor(bool wanted) override;
  void present(const FrameView& view) override;
  bool open() const override;
  float frame_seconds() const override;
  void apply_display(const DisplaySettings& display) override;
  std::vector<SurfaceSize> display_modes() const override;

  // How the game is shown now (the player's display settings it was given last).
  const DisplaySettings& shown() const { return shown_; }

  // Whether a recording broke off: the encoder stopped before the game did.
  bool recording_failed() const { return recording_failed_ || (recording() && recorder_->failed()); }

  // The keyboard, mouse and gamepad of the window.
  InputSource& devices() { return devices_; }

 private:
  GameWindowOptions options_;
  GameSettings game_;
  DisplaySettings shown_;
  Window window_;
  Renderer renderer_;  // (made after the window, released before it)
  FramePainter painter_;
  RaylibInputSource devices_;
  // While recording: what the game is drawn into, and where its frames go.
  RenderTexture2D recorded_{};
  std::unique_ptr<VideoRecorder> recorder_;

  // While the world is drawn at another size than the window's (DisplaySettings::render_scale):
  // what it is drawn into, before it is stretched over the window.
  RenderTexture2D scaled_{};
  float render_scale_ = kFullRenderScale;
  bool scaling_refused_ = false;  // the card refused the picture at this scale: the world is drawn unscaled

  bool recording() const { return recorder_ != nullptr; }
  // Whether the world is drawn at another size than the window's.
  bool draws_scaled() const { return scales_picture(render_scale_) && !scaling_refused_; }
  // A phone's screen: the window is the screen, in its one mode and at its one size.
  bool fills_a_screen() const { return options_.screen_aspect > 0.0f; }
  float recorded_frame_ = 0.0f;  // seconds each recorded frame advances the game by
  bool recording_only_ = false;  // the run records for a set time: nothing else is played
  bool recording_failed_ = false;
  void start_recording(const RecordOptions& record);
  void stop_recording();
  void present_recorded(const FrameView& view);
  void present_scaled(const FrameView& view);
  bool fit_scaled(SurfaceSize asked);
  void reshape(const DisplaySettings& display);
  void follow_render_scale(float scale);
  // What the window shows once the frame is drawn, then shown: the frame rate, whoever wants its picture.
  void finish_frame(bool statistics_overlay);
  // A picture drawn off screen, fitted into the window.
  void show_picture(const RenderTexture2D& picture);
  void point_into_recorded();
};

// Plays a game in a window of its own, with the machine's devices and sound, until the window
// is closed or the game quits. Returns the process exit code.
int play_in_window(const GameWindowOptions& options);
// The same, with the game reading `input` instead of the window's own devices: for a program
// whose screen brings more than a keyboard, a mouse and a gamepad (a touch screen).
int play_in_window(const GameWindowOptions& options, InputSource& input);

}  // namespace redgem
