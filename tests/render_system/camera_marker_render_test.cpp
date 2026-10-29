// System test: the platformer in a real window, its marker (Camera.world_to_screen) drawn over the
// coin nearest the camera, where the picture shows that coin: at the window's size, with the world
// drawn at a share of it, and in a window of another shape.
#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>

#include <raylib.h>

#include "app/engine.hpp"
#include "app/window_presenter.hpp"

using namespace redgem;

namespace {

constexpr float kFrame = 1.0f / 60.0f;
constexpr int kSettleFrames = 90;         // the window comes to its size, the camera to its place
constexpr int kMarkerReach = 12;          // pixels around where the marker should be that are looked at
constexpr int kCoinReach = 5;             // and around where the coin should be
constexpr float kMarkerTolerance = 1.5f;  // pixels the marker's middle may be off
constexpr int kLeastMarkerPixels = 60;    // of the 14 pixels wide round marker
constexpr int kGoldBright = 140;          // red and green above this in a lit gold pixel (the grass is less red)
constexpr float kGoldBlue = 0.65f;        // and blue below this share of the lesser of them (the sky is bluer)
constexpr float kHalfScale = 0.5f;
constexpr float kOddScale = 0.63f;
constexpr SurfaceSize kWide{720, 480};
constexpr SurfaceSize kSquare{500, 500};

// The marker's pink (CoinMarker::COLOR: 255, 64, 160), as the window shows it.
bool is_marker(::Color c) { return c.r > 230 && c.g < 100 && c.b > 130 && c.b < 190; }

// The gold of a coin, lit: as yellow as it is bright, unlike the green grass and the blue sky.
bool is_gold(::Color c) {
  const bool bright = c.r > kGoldBright && c.g > kGoldBright;
  return bright && static_cast<float>(c.b) < kGoldBlue * static_cast<float>(std::min(c.r, c.g));
}

struct Spot {
  float x = 0.0f;
  float y = 0.0f;
};

// The window's last picture, as its pixels.
struct Shot {
  int width = 0;
  int height = 0;
  std::vector<::Color> pixels;
  ::Color at(int x, int y) const { return pixels[static_cast<std::size_t>(y * width + x)]; }
  bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
};

// Where the pixels `wanted` picks lie on average, within `reach` of a spot, and how many there are.
struct Found {
  Spot middle;
  int count = 0;
};
Found find_near(const Shot& shot, Spot spot, int reach, bool (*wanted)(::Color)) {
  Found found;
  const int cx = static_cast<int>(std::lround(spot.x));
  const int cy = static_cast<int>(std::lround(spot.y));
  for (int y = cy - reach; y <= cy + reach; ++y) {
    for (int x = cx - reach; x <= cx + reach; ++x) {
      if (!shot.inside(x, y) || !wanted(shot.at(x, y))) continue;
      found.middle.x += static_cast<float>(x);
      found.middle.y += static_cast<float>(y);
      ++found.count;
    }
  }
  if (found.count > 0) found.middle = {found.middle.x / found.count, found.middle.y / found.count};
  return found;
}

// A pair of numbers a script answered with, [x, y].
Spot spot_of(Engine& engine, const char* code) {
  const RubyResult result = engine.scripts().eval(code, "camera_marker_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(mrb_array_p(result.value));
  mrb_state* mrb = engine.scripts().vm().state();
  return {static_cast<float>(mrb_as_float(mrb, mrb_ary_ref(mrb, result.value, 0))),
          static_cast<float>(mrb_as_float(mrb, mrb_ary_ref(mrb, result.value, 1)))};
}

class MarkedGame {
 public:
  MarkedGame() {
    options_.game_dir = std::filesystem::path(REDGEM_GAMES_DIR) / "platformer";
    options_.capture_cursor = false;
    options_.on_frame_drawn = [this] { take_shot(); };
    window_ = std::make_unique<WindowPresenter>(options_);
    EngineOptions game;
    game.game_dir = options_.game_dir;
    game.presenter = window_.get();
    game.fixed_step = true;
    auto created = Engine::create(game);
    REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
    engine_ = std::move(std::get<std::unique_ptr<Engine>>(created));
  }

  // Shows the game as a player's settings would, and plays until it settles.
  void show(SurfaceSize window, float render_scale) {
    DisplaySettings display = engine_->script_context().display;
    display.resolution = window;
    display.render_scale = render_scale;
    window_->apply_display(display);
    for (int frame = 0; frame < kSettleFrames; ++frame) engine_->frame(kFrame);
    shooting_ = true;
    engine_->frame(kFrame);
    shooting_ = false;
  }

  // Checks the marker is drawn where world_to_screen put it, over the coin where the picture shows it.
  void check_marker_over_coin() {
    const Spot marker = spot_of(*engine_, "Camera.main.get_component(CoinMarker).spot");
    const Spot coin = spot_of(*engine_, "Camera.world_to_screen(Camera.main.get_component(CoinMarker).coin)");
    const Found drawn = find_near(shot_, marker, kMarkerReach, is_marker);
    CHECK(drawn.count >= kLeastMarkerPixels);
    CHECK(std::abs(drawn.middle.x - marker.x) < kMarkerTolerance);
    CHECK(std::abs(drawn.middle.y - marker.y) < kMarkerTolerance);
    CHECK(std::abs(marker.x - coin.x) < kMarkerTolerance);  // over it: straight above
    CHECK(marker.y < coin.y);
    CHECK(find_near(shot_, coin, kCoinReach, is_gold).count > 0);
  }

 private:
  void take_shot() {
    if (!shooting_) return;
    Image screen = LoadImageFromScreen();
    ::Color* colors = LoadImageColors(screen);
    shot_ = {screen.width, screen.height, std::vector<::Color>(colors, colors + screen.width * screen.height)};
    UnloadImageColors(colors);
    UnloadImage(screen);
  }

  GameWindowOptions options_;
  std::unique_ptr<WindowPresenter> window_;
  std::unique_ptr<Engine> engine_;
  bool shooting_ = false;
  Shot shot_;
};

}  // namespace

TEST_CASE("the platformer's coin marker sits over the coin, at any window size and render scale") {
  MarkedGame game;
  game.show(kWide, kFullRenderScale);
  game.check_marker_over_coin();
  game.show(kWide, kHalfScale);
  game.check_marker_over_coin();
  game.show(kSquare, kOddScale);  // a square window, the world drawn at an odd share of it
  game.check_marker_over_coin();
}
