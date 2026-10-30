// System test: a game's window as its player's display settings make it, in a real window: the
// size asked for, and the world drawn at the render scale's share of it and stretched over it,
// the picture shown always the window's size and the card never refusing it.
#include <doctest/doctest.h>

#include <filesystem>
#include <memory>

#include <raylib.h>

#include "app/engine.hpp"
#include "app/window_presenter.hpp"
#include "core/log.hpp"

using namespace redgem;

namespace {

constexpr float kFrame = 1.0f / 60.0f;
constexpr int kSettleFrames = 30;  // the most a window may take to come to a new size
constexpr SurfaceSize kWindow{400, 320};

// The empty game, shown as its game.json says (nothing a player kept).
GameWindowOptions empty_game() {
  GameWindowOptions options;
  options.game_dir = std::filesystem::path(REDGEM_TEMPLATES_DIR) / "empty";
  options.capture_cursor = false;
  return options;
}

// Plays frames until the window is the size asked for (or kSettleFrames have gone by).
void settle(Engine& engine, const WindowPresenter& window, SurfaceSize size) {
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    if (window.surface_size() == size) break;
    engine.frame(kFrame);
  }
  engine.frame(kFrame);
}

}  // namespace

TEST_CASE("the window takes the size its player asks for, and the world is drawn at the render scale's share") {
  int warnings = 0;  // (a picture the card refused is said)
  log::Sink previous = log::set_sink([&warnings](log::Level level, const std::string&) {
    if (level == log::Level::Warning) ++warnings;
  });
  GameWindowOptions options = empty_game();
  SurfaceSize shown;  // the size of the window's picture, as the last frame left it
  options.on_frame_drawn = [&shown] {
    Image screen = LoadImageFromScreen();
    shown = {screen.width, screen.height};
    UnloadImage(screen);
  };
  WindowPresenter window(options);
  EngineOptions game;
  game.game_dir = options.game_dir;
  game.presenter = &window;
  game.fixed_step = true;
  auto created = Engine::create(game);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);

  DisplaySettings display = engine.script_context().display;
  CHECK(display.render_scale == kFullRenderScale);
  display.resolution = kWindow;
  window.apply_display(display);
  settle(engine, window, kWindow);
  CHECK(window.surface_size() == kWindow);
  CHECK(shown == kWindow);

  display.render_scale = 0.5f;
  window.apply_display(display);
  engine.frame(kFrame);
  CHECK(window.surface_size() == kWindow);  // the interface keeps the window's size
  CHECK(window.picture_size() ==
        SurfaceSize{kWindow.width / 2, kWindow.height / 2});  // and says what the world is drawn at
  CHECK(shown == kWindow);

  display.render_scale = 2.0f;
  window.apply_display(display);
  engine.frame(kFrame);
  CHECK(shown == kWindow);
  display.render_scale = 1.0f;  // back to the window's own size: the scaled picture is let go
  window.apply_display(display);
  engine.frame(kFrame);
  CHECK(shown == kWindow);
  CHECK(warnings == 0);
  CHECK(window.shown().render_scale == kFullRenderScale);
  log::set_sink(std::move(previous));
}
