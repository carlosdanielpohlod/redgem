#include "app/scene_files.hpp"
#include "app/window_presenter.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "app/game_display.hpp"
#include "audio/audio.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"

namespace redgem {

WindowSettings game_window_settings(const GameSettings& settings, const DisplaySettings& display,
                                    const GameWindowOptions& options) {
  WindowSettings window;
  window.width = display.resolution.width;
  window.height = display.resolution.height;
  window.title = settings.title;
  window.gpu = options.gpu;
  window.vsync = display.vsync;
  window.frames_per_second = held_fps_cap(display.fps_cap);
  window.mode = display.mode;
  if (options.screen_aspect > 0.0f) {
    window.height = settings.window_height;
    window.width = static_cast<int>(std::lround(static_cast<float>(settings.window_height) * options.screen_aspect));
    window.mode = WindowMode::Windowed;
  }
  return window;
}

namespace {

constexpr ::Color kBlackBars{0, 0, 0, 255};  // around a recorded picture of another shape than the window
constexpr ::Color kUntinted{255, 255, 255, 255};

// Whether a picture drawn off screen is there, at that size.
bool holds_size(const RenderTexture2D& picture, SurfaceSize size) {
  return picture.id != 0 && SurfaceSize{picture.texture.width, picture.texture.height} == size;
}

}  // namespace

namespace {

// The window of a game, as its settings and its player's choices say.
WindowSettings window_for(const GameWindowOptions& options) {
  return game_window_settings(read_game_settings(options.game_dir),
                              game_display(options.game_dir, player_settings_file(options.settings_file)), options);
}

}  // namespace

WindowPresenter::WindowPresenter(GameWindowOptions options)
    : options_(std::move(options)), window_(window_for(options_)), renderer_(window_), painter_(renderer_) {
  apply_display(game_display(options_.game_dir, player_settings_file(options_.settings_file)));
  if (options_.record) start_recording(*options_.record);
}

WindowPresenter::~WindowPresenter() {
  recorder_.reset();  // (its last frames are read back before the picture they come from goes)
  if (recorded_.id != 0) UnloadRenderTexture(recorded_);
  if (scaled_.id != 0) UnloadRenderTexture(scaled_);
}

void WindowPresenter::apply_display(const DisplaySettings& display) {
  if (fills_a_screen()) {
    window_.pace(display);
  } else {
    window_.show_as(display);
  }
  renderer_.set_quality(quality_caps(display));
  render_scale_ = held_render_scale(display.render_scale);
}

SurfaceSize WindowPresenter::world_size() const {
  if (recording()) return surface_size();
  const bool scaled = scales_picture(render_scale_) && scaled_.id != 0;
  return scaled ? SurfaceSize{scaled_.texture.width, scaled_.texture.height} : window_.drawing_size();
}

std::vector<SurfaceSize> WindowPresenter::display_modes() const {
  if (fills_a_screen()) return {window_.drawing_size()};
  return window_sizes_for(window_.monitor_size());
}

void WindowPresenter::start_recording(const RecordOptions& record) {
  const SurfaceSize window = window_.drawing_size();
  const FrameSize size = even(record.size.value_or(FrameSize{window.width, window.height}));
  recorded_ = LoadRenderTexture(size.width, size.height);
  recorder_ = std::make_unique<VideoRecorder>(record, size);
  recorded_frame_ = recorded_frame_seconds(record.fps);
  recording_only_ = record.seconds.has_value();
}

// What is recorded so far stays a video; the game goes on in the window, in real time.
void WindowPresenter::stop_recording() {
  recording_failed_ = recorder_->failed();
  recorder_.reset();
  renderer_.set_clock(std::nullopt);
  SetMouseOffset(0, 0);
  SetMouseScale(1.0f, 1.0f);
}

SurfaceSize WindowPresenter::surface_size() const {
  if (!recording()) return window_.drawing_size();
  return {recorded_.texture.width, recorded_.texture.height};
}

bool WindowPresenter::open() const {
  const bool recorded_enough = recording() && recorder_->complete();
  // A run that was only a recording ends when the recording breaks off, with an error.
  const bool nothing_left_to_record = recording_failed_ && recording_only_;
  return !window_.close_requested() && !recorded_enough && !nothing_left_to_record;
}

float WindowPresenter::frame_seconds() const { return recording() ? recorded_frame_ : window_.frame_seconds(); }

void WindowPresenter::capture_cursor(bool wanted) {
  if (options_.capture_cursor) window_.set_cursor_locked(wanted);
}

void WindowPresenter::present(const FrameView& view) {
  if (view.stop_recording && recording()) stop_recording();
  if (recording()) {
    present_recorded(view);
    return;
  }
  if (scales_picture(render_scale_)) {
    present_scaled(view);
    return;
  }
  window_.begin_frame();
  painter_.paint(view);
  finish_frame();
}

void WindowPresenter::finish_frame() {
  if (options_.show_fps) window_.draw_frame_rate();
  rlDrawRenderBatchActive();  // (whoever wants the frame's picture finds it drawn)
  if (options_.on_frame_drawn) options_.on_frame_drawn();
  window_.end_frame();
}

// The world at its scale, stretched over the window; the interface over it at the window's size.
void WindowPresenter::present_scaled(const FrameView& view) {
  fit_scaled(scaled_size(window_.drawing_size(), render_scale_));
  painter_.paint_world(view, &scaled_);
  window_.begin_frame();
  show_picture(scaled_);
  painter_.paint_flat(view);
  finish_frame();
}

void WindowPresenter::fit_scaled(SurfaceSize size) {
  if (holds_size(scaled_, size)) return;
  if (scaled_.id != 0) UnloadRenderTexture(scaled_);
  scaled_ = LoadRenderTexture(size.width, size.height);
  SetTextureFilter(scaled_.texture, TEXTURE_FILTER_BILINEAR);  // (smooth when it is stretched)
}

// The game's clock is the recording's: what moves with time (the wind, film grain) moves as fast
// in the video as in the game, however long a frame took to draw.
void WindowPresenter::present_recorded(const FrameView& view) {
  renderer_.set_clock(static_cast<double>(recorder_->frames()) * recorded_frame_);
  point_into_recorded();
  painter_.paint(view, &recorded_);
  recorder_->add_frame(recorded_.id);
  if (recorder_->failed()) stop_recording();  // (the game goes on in the window)
  window_.begin_frame();
  show_picture(recorded_);
  finish_frame();
}

void WindowPresenter::show_picture(const RenderTexture2D& shown) {
  ClearBackground(kBlackBars);
  const SurfaceSize window = window_.drawing_size();
  const FrameSize picture{shown.texture.width, shown.texture.height};
  const Letterbox box = letterbox(FrameSize{window.width, window.height}, picture);
  const Rectangle from{0.0f, 0.0f, static_cast<float>(picture.width), -static_cast<float>(picture.height)};
  DrawTexturePro(shown.texture, from, Rectangle{box.x, box.y, box.width, box.height}, Vector2{0.0f, 0.0f}, 0.0f,
                 kUntinted);
}

// The pointer, over the window, points into the recorded picture.
void WindowPresenter::point_into_recorded() {
  const SurfaceSize window = window_.drawing_size();
  const PointMapping mapping = window_to_picture(FrameSize{window.width, window.height},
                                                 FrameSize{recorded_.texture.width, recorded_.texture.height});
  SetMouseOffset(static_cast<int>(mapping.offset_x), static_cast<int>(mapping.offset_y));
  SetMouseScale(mapping.scale_x, mapping.scale_y);
}

namespace {

// The game's first scene; nothing, with the reason logged, when it cannot be read.
std::optional<SceneDesc> first_scene(const std::filesystem::path& game_dir) {
  auto scene = read_game_scene(game_dir);
  if (auto* read = std::get_if<SceneDesc>(&scene)) return std::move(*read);
  log::error(std::get<std::string>(scene));
  return std::nullopt;
}

// Plays the game of `window` in it from `scene`, read from `input`, with the machine's sound.
int play(WindowPresenter& window, InputSource& input, const GameWindowOptions& options, SceneDesc scene) {
  const std::unique_ptr<AudioSystem> audio = create_raylib_audio();
  EngineOptions game;
  game.game_dir = options.game_dir;
  game.presenter = &window;
  game.input = &input;
  game.audio = audio.get();
  game.settings_file = options.settings_file;
  const int status = Engine::create(game, std::move(scene))->run();
  return window.recording_failed() ? kExitFailure : status;
}

// The scene is read before the window is made: a game that cannot start says so with no window
// opening and closing first. Then the window, first and last: the engine shows, reads and sounds
// through what is made here. The game is read from `input`; from the window's own devices when
// there is none.
int play_from_first_scene(const GameWindowOptions& options, InputSource* input) {
  std::optional<SceneDesc> scene = first_scene(options.game_dir);
  if (!scene) return kExitFailure;
  WindowPresenter window(options);
  return play(window, input != nullptr ? *input : window.devices(), options, std::move(*scene));
}

}  // namespace

int play_in_window(const GameWindowOptions& options) { return play_from_first_scene(options, nullptr); }

int play_in_window(const GameWindowOptions& options, InputSource& input) {
  return play_from_first_scene(options, &input);
}

}  // namespace redgem
