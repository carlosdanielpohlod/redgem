// The interface scales with the screen: a layout given in units of the game's reference screen
// (here the engine's window, 1280x720, its height matched) takes the same share of the screen on
// any screen, its texts are drawn at the pixels they take there, clicks land on what is drawn,
// and the player's own scale multiplies it all.
#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <cmath>
#include <string>
#include <vector>

#include "app/app_config.hpp"
#include "app/engine.hpp"
#include "core/config.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr SurfaceSize kScreens[] = {{1280, 720}, {1920, 1080}, {3840, 2160}, {800, 600}};
constexpr SurfaceSize kUhd{3840, 2160};
constexpr int kReferenceHeight = config::kWindowHeight;
constexpr int kTextUnits = 36;
constexpr float kPixelTolerance = 1.5f;          // each edge is rounded to a pixel
constexpr float kAcross[] = {0.0f, 1.0f, 0.5f};  // where each rectangle's anchor is: left, right, middle
constexpr float kDown[] = {0.0f, 1.0f, 0.5f};
constexpr std::size_t kRectangles = 3;

// A HUD: a box at the top-left corner, one at the bottom-right corner, one in the middle, and a
// text at the top-right corner.
constexpr const char* kLayout =
    "UI.rect(anchor: :top_left, x: 20, y: 20, width: 300, height: 60);"
    "UI.rect(anchor: :bottom_right, x: -20, y: -20, width: 200, height: 100);"
    "UI.rect(anchor: :center, x: 0, y: 0, width: 400, height: 200);"
    "UI.text('Score', anchor: :top_right, x: -20, y: 20, size: 36)";
constexpr const char* kButton = "UI.button('Go', x: 100, y: 50, width: 200, height: 60)";

SceneDesc empty_scene() {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  desc.nodes = {camera};
  return desc;
}

std::unique_ptr<Engine> engine_on(const SceneDesc& scene, Presenter& presenter) {
  EngineOptions options;
  options.game_dir = fixture("games/directed");
  options.fixed_step = true;
  options.scene = &scene;
  options.presenter = &presenter;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

void frame(Engine& engine) { engine.frame(config::kFixedDeltaSeconds); }

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

// A rectangle as shares of the screen's height: its size, and how far it is from its anchor
// (the point of the screen it sits at, by the matching corner of its own).
struct Relative {
  float from_anchor_x, from_anchor_y, width, height;
};

Relative relative(const HudItem& item, SurfaceSize screen, std::size_t index) {
  const float tall = static_cast<float>(screen.height);
  const float anchor_x = kAcross[index] * static_cast<float>(screen.width - item.width);
  const float anchor_y = kDown[index] * static_cast<float>(screen.height - item.height);
  return {(static_cast<float>(item.x) - anchor_x) / tall, (static_cast<float>(item.y) - anchor_y) / tall,
          static_cast<float>(item.width) / tall, static_cast<float>(item.height) / tall};
}

// The layout's rectangles, drawn on a screen of that size.
std::vector<HudItem> drawn_on(SurfaceSize screen) {
  const SceneDesc scene = empty_scene();
  NullPresenter presenter(screen);
  auto engine = engine_on(scene, presenter);
  frame(*engine);
  engine->script_context().hud.clear();
  ruby(*engine, kLayout);
  return engine->script_context().hud;
}

void check_same(const Relative& one, const Relative& other, SurfaceSize screen) {
  const float tolerance = kPixelTolerance / static_cast<float>(screen.height);
  CHECK(std::abs(one.from_anchor_x - other.from_anchor_x) <= tolerance);
  CHECK(std::abs(one.from_anchor_y - other.from_anchor_y) <= tolerance);
  CHECK(std::abs(one.width - other.width) <= tolerance);
  CHECK(std::abs(one.height - other.height) <= tolerance);
}

}  // namespace

TEST_CASE("the same layout takes the same share of the screen at 720p, 1080p, 4K and 800x600") {
  const SurfaceSize reference = kScreens[0];
  const std::vector<HudItem> at_reference = drawn_on(reference);
  REQUIRE(at_reference.size() == kRectangles + 1);
  for (const SurfaceSize screen : kScreens) {
    CAPTURE(screen.width);
    const std::vector<HudItem> hud = drawn_on(screen);
    REQUIRE(hud.size() == kRectangles + 1);
    for (std::size_t index = 0; index < kRectangles; ++index) {
      check_same(relative(hud[index], screen, index), relative(at_reference[index], reference, index), screen);
    }
    // On a screen of the reference's shape, the same share of its width and height too.
    const bool same_shape = screen.width * reference.height == reference.width * screen.height;
    const float share = static_cast<float>(hud[1].x) / static_cast<float>(screen.width);
    const float reference_share = static_cast<float>(at_reference[1].x) / static_cast<float>(reference.width);
    if (same_shape) CHECK(std::abs(share - reference_share) <= kPixelTolerance / static_cast<float>(screen.width));
  }
}

TEST_CASE("a text is drawn at the pixels its size takes on the screen, and measured there") {
  for (const SurfaceSize screen : kScreens) {
    CAPTURE(screen.width);
    const HudItem text = drawn_on(screen).back();
    CHECK(text.kind == HudItem::Kind::Text);
    const int pixels = static_cast<int>(std::lround(kTextUnits * static_cast<float>(screen.height) / kReferenceHeight));
    CHECK(text.size == pixels);
    // Its right edge is 20 units from the screen's.
    const float scale = static_cast<float>(screen.height) / kReferenceHeight;
    const int width = static_cast<int>(std::lround(estimated_text_width("Score", pixels) / scale));
    CHECK(std::abs(text.x - (screen.width - std::lround((20 + width) * scale))) <= 2);
  }
}

TEST_CASE("the screen, the pointer and the scale are told in units of the reference") {
  const SceneDesc scene = empty_scene();
  NullPresenter presenter(kUhd);
  auto engine = engine_on(scene, presenter);
  engine->scripted_input().move_mouse(600.0f, 240.0f);
  frame(*engine);
  CHECK(ruby(*engine, "UI.scale") == "3.0");
  CHECK(ruby(*engine, "[UI.width, UI.height]") == "[1280, 720]");
  CHECK(ruby(*engine, "[UI.pixel_width, UI.pixel_height]") == "[3840, 2160]");
  CHECK(ruby(*engine, "UI.reference") == "[1280, 720]");
  CHECK(ruby(*engine, "[UI.mouse_x, UI.mouse_y]") == "[200.0, 80.0]");
  // The camera's points of the screen are in units too: its middle is the middle of 1280x720.
  CHECK(ruby(*engine, "Camera.world_to_screen(Vector3.new(0, 0, -10)).map(&:round)") == "[640, 360]");
  CHECK(ruby(*engine, "Camera.screen_ray(640, 360)[1].approx?(Vector3.forward, 0.001)") == "true");
}

TEST_CASE("a click or a tap lands on a button where it is drawn, at any size") {
  const SceneDesc scene = empty_scene();
  NullPresenter presenter(kUhd);
  auto engine = engine_on(scene, presenter);
  ScriptedInputSource& input = engine->scripted_input();
  const auto clicked_at = [&](float x, float y) {
    input.move_mouse(x, y);
    input.set_mouse_button(MouseButton::Left, true);
    frame(*engine);
    input.set_mouse_button(MouseButton::Left, false);
    frame(*engine);
    return ruby(*engine, kButton);
  };
  CHECK(clicked_at(600.0f, 240.0f) == "true");  // inside the button, three pixels a unit
  CHECK(clicked_at(150.0f, 90.0f) == "false");  // where it would be if a unit were a pixel
  CHECK(clicked_at(880.0f, 320.0f) == "true");  // its far corner grew with it
}

TEST_CASE("the player's scale multiplies the screen's, and the game's menu follows it") {
  const SceneDesc scene = empty_scene();
  NullPresenter presenter(kUhd);
  auto engine = engine_on(scene, presenter);
  frame(*engine);
  ruby(*engine, "Settings.ui_scale = 1.5");
  CHECK(ruby(*engine, "UI.scale") == "4.5");
  CHECK(ruby(*engine, "UI.height") == "480");
  CHECK(ruby(*engine, "Settings.ui_scale = 7; Settings.ui_scale") == "2.0");  // held to twice
  ruby(*engine, "Settings.ui_scale = 1.5");
  engine->script_context().hud.clear();
  ruby(*engine, "UI.text('A', x: 0, y: 0, size: 20)");
  CHECK(engine->script_context().hud.back().size == 90);
  // The menu's sizes are for a 720 unit tall screen: with fewer units, it shrinks to fit them.
  CHECK(ruby(*engine, "(GameMenu.new.scale - 480.0 / 720).abs < 0.001") == "true");
}
