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

bool loaded(const RenderTexture2D& picture) { return picture.id != 0; }

// Whether a picture drawn off screen is there, at that size.
bool holds_size(const RenderTexture2D& picture, SurfaceSize size) {
  return loaded(picture) && SurfaceSize{picture.texture.width, picture.texture.height} == size;
}

// Whether a picture to draw into was made whole: the card may refuse one (too big for it, no memory).
bool drawable(const RenderTexture2D& picture) { return loaded(picture) && rlFramebufferComplete(picture.id); }

void release(RenderTexture2D& picture) {
  if (loaded(picture)) UnloadRenderTexture(picture);
  picture = RenderTexture2D{};
}

}  // namespace

WindowPresenter::WindowPresenter(GameWindowOptions options)
    : options_(std::move(options)),
      game_(read_game_settings(options_.game_dir)),
      shown_(options_.display.value_or(game_.display)),
      window_(game_window_settings(game_, shown_, options_)),
      renderer_(window_),
      painter_(renderer_) {
  renderer_.set_quality(quality_caps(shown_));
  render_scale_ = held_render_scale(shown_.render_scale);
  if (options_.record) start_recording(*options_.record);
}

WindowPresenter::~WindowPresenter() {
  recorder_.reset();  // (its last frames are read back before the picture they come from goes)
  release(recorded_);
  release(scaled_);
}

// Only what changed is done again: a script may write the settings every frame (a slider dragged).
void WindowPresenter::apply_display(const DisplaySettings& display) {
  if (!same_shape(display, shown_)) reshape(display);
  if (!same_pace(display, shown_)) window_.pace(display);
  if (quality_caps(display) != quality_caps(shown_)) renderer_.set_quality(quality_caps(display));
  if (display.render_scale != shown_.render_scale) follow_render_scale(display.render_scale);
  shown_ = display;
}

void WindowPresenter::reshape(const DisplaySettings& display) {
  if (!fills_a_screen()) window_.reshape(display.mode, display.resolution);
}

// A scale given up on (see fit_scaled) is tried again once another is asked for.
void WindowPresenter::follow_render_scale(float scale) {
  render_scale_ = held_render_scale(scale);
  scaling_refused_ = false;
  if (!scales_picture(render_scale_)) release(scaled_);
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
  if (draws_scaled() && fit_scaled(scaled_size(window_.drawing_size(), render_scale_))) {
    present_scaled(view);
    return;
  }
  window_.begin_frame();
  painter_.paint(view);
  finish_frame(view.statistics_overlay);
}

void WindowPresenter::finish_frame(bool statistics_overlay) {
  const bool basic_counter = options_.show_fps && !statistics_overlay;
  if (basic_counter) window_.draw_frame_rate();
  rlDrawRenderBatchActive();  // (whoever wants the frame's picture finds it drawn)
  if (options_.on_frame_drawn) options_.on_frame_drawn();
  window_.end_frame();
}

// The world at its scale, stretched over the window; the interface over it at the window's size.
void WindowPresenter::present_scaled(const FrameView& view) {
  painter_.paint_world(view, &scaled_);
  window_.begin_frame();
  show_picture(scaled_);
  painter_.paint_flat(view);
  finish_frame(view.statistics_overlay);
}

// Makes the picture the world is drawn into at that size, held to the most the card can draw
// into. One the card refuses makes the world drawn at the window's size, with a warning, until
// another scale is asked for. Whether there is one to draw into.
bool WindowPresenter::fit_scaled(SurfaceSize asked) {
  const SurfaceSize size = held_to_side(asked, window_.largest_picture_side());
  if (holds_size(scaled_, size)) return true;
  release(scaled_);
  scaled_ = LoadRenderTexture(size.width, size.height);
  if (drawable(scaled_)) {
    SetTextureFilter(scaled_.texture, TEXTURE_FILTER_BILINEAR);  // (smooth when it is stretched)
    return true;
  }
  release(scaled_);
  scaling_refused_ = true;
  log::warning("the graphics card cannot draw the world at " + std::to_string(size.width) + "x" +
               std::to_string(size.height) + ": it is drawn at the window's size");
  return false;
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
  finish_frame(view.statistics_overlay);
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
  game.display = window.shown();
  game.display_kept_in = options.display_kept_in;
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
