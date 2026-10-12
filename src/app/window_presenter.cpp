#include "app/scene_files.hpp"
#include "app/window_presenter.hpp"

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

WindowPresenter::WindowPresenter(GameWindowOptions options)
    : options_(std::move(options)),
      window_(game_window_settings(read_game_settings(options_.game_dir), options_)),
      renderer_(window_),
      painter_(renderer_) {}

void WindowPresenter::capture_cursor(bool wanted) {
  if (options_.capture_cursor) window_.set_cursor_locked(wanted);
}

void WindowPresenter::present(const FrameView& view) {
  window_.begin_frame();
  painter_.paint(view);
  if (options_.show_fps) window_.draw_frame_rate();
  if (options_.on_frame_drawn) options_.on_frame_drawn();
  window_.end_frame();
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

}  // namespace

// The scene is read before the window is made: a game that cannot start says so with no window
// opening and closing first. Then the window, first and last: the engine shows, reads and sounds
// through what is made here.
int play_in_window(const GameWindowOptions& options) {
  std::optional<SceneDesc> scene = first_scene(options.game_dir);
  if (!scene) return kExitFailure;
  WindowPresenter window(options);
  return play(window, window.devices(), options.game_dir, std::move(*scene));
}

int play_in_window(const GameWindowOptions& options, InputSource& input) {
  std::optional<SceneDesc> scene = first_scene(options.game_dir);
  if (!scene) return kExitFailure;
  WindowPresenter window(options);
  return play(window, input, options.game_dir, std::move(*scene));
}

}  // namespace redgem
