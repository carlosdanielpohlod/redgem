#include "app/scene_files.hpp"
#include "app/window_presenter.hpp"

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

WindowSettings game_window_settings(const GameSettings& settings, const GameWindowOptions& options) {
  WindowSettings window;
  window.width = settings.window_width;
  window.height = settings.window_height;
  window.title = settings.title;
  window.gpu = options.gpu;
  if (options.screen_aspect > 0.0f) {
    window.width = static_cast<int>(std::lround(static_cast<float>(settings.window_height) * options.screen_aspect));
  }
  return window;
}

namespace {

constexpr ::Color kBlackBars{0, 0, 0, 255};  // around a recorded picture of another shape than the window
constexpr ::Color kUntinted{255, 255, 255, 255};
constexpr float kHalfOf = 0.5f;  // of the room left: what goes on each side

// The largest rectangle of `picture`'s shape that fits in `window`, in its middle.
Rectangle fitted(SurfaceSize window, SurfaceSize picture) {
  const float scale = std::min(static_cast<float>(window.width) / static_cast<float>(picture.width),
                               static_cast<float>(window.height) / static_cast<float>(picture.height));
  const float width = static_cast<float>(picture.width) * scale;
  const float height = static_cast<float>(picture.height) * scale;
  return {(static_cast<float>(window.width) - width) * kHalfOf, (static_cast<float>(window.height) - height) * kHalfOf,
          width, height};
}

}  // namespace

WindowPresenter::WindowPresenter(GameWindowOptions options)
    : options_(std::move(options)),
      window_(game_window_settings(read_game_settings(options_.game_dir), options_)),
      renderer_(window_),
      painter_(renderer_) {
  if (options_.record) start_recording(*options_.record);
}

WindowPresenter::~WindowPresenter() {
  recorder_.reset();  // (its last frames are read back before the picture they come from goes)
  if (recorded_.id != 0) UnloadRenderTexture(recorded_);
}

void WindowPresenter::start_recording(const RecordOptions& record) {
  const SurfaceSize window = window_.drawing_size();
  const FrameSize size = record.size.value_or(FrameSize{window.width, window.height});
  recorded_ = LoadRenderTexture(size.width, size.height);
  recorder_ = std::make_unique<VideoRecorder>(record, size);
  recorded_frame_ = recorded_frame_seconds(record.fps);
}

// What is recorded so far stays a video; the game goes on in the window, in real time.
void WindowPresenter::stop_recording() {
  recorder_.reset();
  renderer_.set_clock(std::nullopt);
}

SurfaceSize WindowPresenter::surface_size() const {
  if (!recording()) return window_.drawing_size();
  return {recorded_.texture.width, recorded_.texture.height};
}

bool WindowPresenter::open() const {
  const bool recorded_enough = recording() && recorder_->complete();
  return !window_.close_requested() && !recorded_enough;
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
  window_.begin_frame();
  painter_.paint(view);
  if (options_.show_fps) window_.draw_frame_rate();
  if (options_.on_frame_drawn) options_.on_frame_drawn();
  window_.end_frame();
}

// The game's clock is the recording's: what moves with time (the wind, film grain) moves as fast
// in the video as in the game, however long a frame took to draw.
void WindowPresenter::present_recorded(const FrameView& view) {
  renderer_.set_clock(static_cast<double>(recorder_->frames()) * recorded_frame_);
  painter_.paint(view, &recorded_);
  recorder_->add_frame(recorded_.id);
  window_.begin_frame();
  show_recorded();
  if (options_.show_fps) window_.draw_frame_rate();
  if (options_.on_frame_drawn) options_.on_frame_drawn();
  window_.end_frame();
}

void WindowPresenter::show_recorded() {
  ClearBackground(kBlackBars);
  const SurfaceSize picture{recorded_.texture.width, recorded_.texture.height};
  const Rectangle from{0.0f, 0.0f, static_cast<float>(picture.width), -static_cast<float>(picture.height)};
  DrawTexturePro(recorded_.texture, from, fitted(window_.drawing_size(), picture), Vector2{0.0f, 0.0f}, 0.0f,
                 kUntinted);
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
int play(WindowPresenter& window, InputSource& input, const std::filesystem::path& game_dir, SceneDesc scene) {
  const std::unique_ptr<AudioSystem> audio = create_raylib_audio();
  EngineOptions game;
  game.game_dir = game_dir;
  game.presenter = &window;
  game.input = &input;
  game.audio = audio.get();
  return Engine::create(game, std::move(scene))->run();
}

// The scene is read before the window is made: a game that cannot start says so with no window
// opening and closing first. Then the window, first and last: the engine shows, reads and sounds
// through what is made here. The game is read from `input`; from the window's own devices when
// there is none.
int play_from_first_scene(const GameWindowOptions& options, InputSource* input) {
  std::optional<SceneDesc> scene = first_scene(options.game_dir);
  if (!scene) return kExitFailure;
  WindowPresenter window(options);
  return play(window, input != nullptr ? *input : window.devices(), options.game_dir, std::move(*scene));
}

}  // namespace

int play_in_window(const GameWindowOptions& options) { return play_from_first_scene(options, nullptr); }

int play_in_window(const GameWindowOptions& options, InputSource& input) {
  return play_from_first_scene(options, &input);
}

}  // namespace redgem
