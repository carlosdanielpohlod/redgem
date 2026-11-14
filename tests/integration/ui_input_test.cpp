#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <optional>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kScreenWidth = 800;
constexpr int kScreenHeight = 600;
constexpr float kMouseX = 150.0f;
constexpr float kMouseY = 90.0f;
constexpr float kMouseStep = 12.0f;
constexpr float kWheelNotches = 2.0f;
constexpr int kPausedFrames = 30;
constexpr float kRayTolerance = 1e-3f;
// A button drawn by the test's Ruby code, and a point inside and outside it.
constexpr const char* kButton = "UI.button('Go', x: 100, y: 50, width: 200, height: 60)";
constexpr float kOutsideX = 500.0f;
constexpr int kMeasuredWidth = 40;  // how wide FontMeasurePresenter says every text in a game's face is

// Nobody draws these games: they are told how big a screen they are on.
NullPresenter screen({kScreenWidth, kScreenHeight});

// A camera looking down -Z and one object, with no scripts of their own.
SceneDesc small_scene() {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  NodeDesc walker;
  walker.name = "Walker";
  NodeDesc tuned;
  tuned.name = "Tuned";
  NodeDesc plain;
  plain.name = "Plain";
  desc.nodes = {camera, walker, tuned, plain};
  return desc;
}

std::unique_ptr<Engine> engine_for(const SceneDesc& scene, Presenter* presenter = &screen) {
  EngineOptions options;
  options.game_dir = fixture("games/directed");
  options.fixed_step = true;
  options.scene = &scene;
  options.presenter = presenter;
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

}  // namespace

TEST_CASE("scripts read the mouse: where it is, how it moved, its buttons and wheel") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  ScriptedInputSource& input = engine->scripted_input();
  input.move_mouse(kMouseX, kMouseY);
  frame(*engine);
  CHECK(ruby(*engine, "[Input.mouse_x, Input.mouse_y]") == "[150.0, 90.0]");
  CHECK(ruby(*engine, "Input.mouse_down?(:left)") == "false");

  input.move_mouse(kMouseX + kMouseStep, kMouseY);
  input.set_mouse_button(MouseButton::Left, true);
  input.set_mouse_wheel(kWheelNotches);
  frame(*engine);
  CHECK(ruby(*engine, "[Input.mouse_dx, Input.mouse_dy]") == "[12.0, 0.0]");
  CHECK(ruby(*engine, "[Input.mouse_down?(:left), Input.mouse_pressed?(:left), Input.mouse_down?(:right)]") ==
        "[true, true, false]");
  CHECK(ruby(*engine, "Input.mouse_wheel") == "2.0");

  frame(*engine);  // still held: no longer "pressed this frame"
  CHECK(ruby(*engine, "[Input.mouse_down?(:left), Input.mouse_pressed?(:left)]") == "[true, false]");
  input.set_mouse_button(MouseButton::Left, false);
  frame(*engine);
  CHECK(ruby(*engine, "[Input.mouse_down?(:left), Input.mouse_released?(:left)]") == "[false, true]");
  CHECK_FALSE(engine->scripts().eval("Input.mouse_down?(:thumb)", "bad_button.rb").ok);
}

TEST_CASE("a button is clicked when the left button is released over it") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  ScriptedInputSource& input = engine->scripted_input();
  input.move_mouse(kMouseX, kMouseY);  // inside the button
  frame(*engine);
  CHECK(ruby(*engine, kButton) == "false");  // hovering is not clicking
  input.set_mouse_button(MouseButton::Left, true);
  frame(*engine);
  CHECK(ruby(*engine, kButton) == "false");  // nor is holding
  input.set_mouse_button(MouseButton::Left, false);
  frame(*engine);
  CHECK(ruby(*engine, kButton) == "true");

  // Released somewhere else: not a click.
  input.move_mouse(kOutsideX, kMouseY);
  input.set_mouse_button(MouseButton::Left, true);
  frame(*engine);
  input.set_mouse_button(MouseButton::Left, false);
  frame(*engine);
  CHECK(ruby(*engine, kButton) == "false");
}

TEST_CASE("texts, rectangles and images are queued for the frame, and the screen size is known") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  frame(*engine);
  CHECK(ruby(*engine, "[UI.width, UI.height]") == "[800, 600]");
  ruby(*engine, "UI.rect(x: 10, y: 20, width: 300, height: 40, color: :ruby, alpha: 128, radius: 8)");
  ruby(*engine, "UI.image(:logo, x: 5, y: 6, width: 64)");
  ruby(*engine, "UI.text('Centered', x: 400, y: 10, size: 20, align: :center)");
  const std::vector<HudItem>& hud = engine->script_context().hud;
  REQUIRE(hud.size() == 3);
  CHECK(hud[0].kind == HudItem::Kind::Rect);
  CHECK(hud[0].width == 300);
  CHECK(hud[0].radius == doctest::Approx(8.0f));
  CHECK(hud[0].color.a == doctest::Approx(128.0f / 255.0f));
  CHECK(hud[1].kind == HudItem::Kind::Image);
  CHECK(hud[1].text == "logo");
  CHECK(hud[2].kind == HudItem::Kind::Text);
  CHECK(hud[2].x < 400);  // moved left by half its width
  CHECK(ruby(*engine, "UI.text_width('abcd', size: 20) > UI.text_width('ab', size: 20)") == "true");
  CHECK_FALSE(engine->scripts().eval("UI.rect(x: 0, y: 0, width: 1, height: 1, color: :plaid)", "bad_color.rb").ok);
}

TEST_CASE("Time.scale 0 pauses the game while behaviours keep being updated") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  frame(*engine);
  frame(*engine);
  const float moving_x = engine->scene().world_transform(engine->scene().find("Walker")).position.x;
  CHECK(moving_x > 0.0f);  // the fixture's scene script makes it walk
  ruby(*engine, "$updates_while_paused = 0; Time.scale = 0");
  const std::string paused_at = ruby(*engine, "Time.elapsed");
  ruby(*engine,
       "class Counter < Behaviour; def update(dt); $updates_while_paused += 1; $paused_delta = dt; end; end;"
       "Scene.find('Plain').add_component(Counter)");
  for (int i = 0; i < kPausedFrames; ++i) frame(*engine);
  CHECK(engine->scene().world_transform(engine->scene().find("Walker")).position.x == doctest::Approx(moving_x));
  CHECK(ruby(*engine, "Time.elapsed") == paused_at);
  CHECK(ruby(*engine, "$updates_while_paused") == std::to_string(kPausedFrames));  // a menu would still work
  CHECK(ruby(*engine, "$paused_delta") == "0.0");

  ruby(*engine, "Time.scale = 1");
  frame(*engine);
  CHECK(engine->scene().world_transform(engine->scene().find("Walker")).position.x > moving_x);
  CHECK_FALSE(engine->scripts().eval("Time.scale = -1", "bad_scale.rb").ok);
}

TEST_CASE("Camera.screen_ray goes through a point of the screen") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  frame(*engine);
  // The middle of the screen is straight ahead; the left edge leans left, the top leans up.
  CHECK(ruby(*engine, "Camera.screen_ray(400, 300)[1].approx?(Vector3.forward, 0.001)") == "true");
  CHECK(ruby(*engine, "Camera.screen_ray(0, 300)[1].x < -0.1") == "true");
  CHECK(ruby(*engine, "Camera.screen_ray(400, 0)[1].y > 0.1") == "true");
  CHECK(ruby(*engine, "Camera.screen_ray(400, 300)[0].approx?(Camera.main.transform.position, 0.001)") == "true");
  static_cast<void>(kRayTolerance);
}

TEST_CASE("scripts set the camera's field of view and the picture's speed streaks") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  frame(*engine);
  ruby(*engine, "Camera.fov = 70");
  CHECK(ruby(*engine, "Camera.fov.round") == "70");
  const ObjectId camera = engine->scene().main_camera();
  CHECK(radians_to_degrees(engine->scene().at(camera).camera->fov_y_radians) == doctest::Approx(70.0f));
  CHECK_FALSE(engine->scripts().eval("Camera.fov = 400", "bad_fov.rb").ok);

  CHECK(engine->script_context().motion_blur == doctest::Approx(0.0f));
  ruby(*engine, "Camera.motion_blur = 0.6");
  CHECK(engine->script_context().motion_blur == doctest::Approx(0.6f));
  CHECK_FALSE(engine->scripts().eval("Camera.motion_blur = 9", "bad_blur.rb").ok);  // 0 to 1, refused past it
  CHECK(engine->script_context().motion_blur == doctest::Approx(0.6f));
}

TEST_CASE("scripts change the scene's auto exposure for the game's camera, and give it back with nil") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  const AutoExposureOverrides& set = engine->script_context().auto_exposure;
  CHECK(ruby(*engine, "Camera.auto_exposure.inspect") == "nil");  // the scene's own
  ruby(*engine, "Camera.auto_exposure = true");
  CHECK(set.enabled == std::optional<bool>(true));
  CHECK(ruby(*engine, "Camera.auto_exposure") == "true");
  CHECK_FALSE(engine->scripts().eval("Camera.auto_exposure = 1", "bad_switch.rb").ok);  // true, false or nil

  ruby(*engine, "Camera.exposure_compensation = -1.5");
  CHECK(ruby(*engine, "Camera.exposure_compensation") == "-1.5");
  CHECK_FALSE(engine->scripts().eval("Camera.exposure_compensation = 99", "bad_ev.rb").ok);
  CHECK(*set.compensation == doctest::Approx(-1.5f));

  ruby(*engine, "Camera.exposure_range = [-2, 3]");
  CHECK(ruby(*engine, "Camera.exposure_range.inspect") == "[-2.0, 3.0]");
  CHECK_FALSE(engine->scripts().eval("Camera.exposure_range = [3, -2]", "bad_range.rb").ok);  // upside down
  CHECK_FALSE(engine->scripts().eval("Camera.exposure_range = 3", "bad_range.rb").ok);
  CHECK(*set.max_ev == doctest::Approx(3.0f));

  ruby(*engine, "Camera.exposure_speed = [4, 0.5]");
  CHECK(*set.speed_down == doctest::Approx(0.5f));
  CHECK_FALSE(engine->scripts().eval("Camera.exposure_speed = [-1, 1]", "bad_speed.rb").ok);

  ruby(*engine,
       "Camera.auto_exposure = nil; Camera.exposure_compensation = nil; Camera.exposure_range = nil; "
       "Camera.exposure_speed = nil");
  CHECK_FALSE(set.enabled.has_value());
  CHECK_FALSE(set.compensation.has_value());
  CHECK_FALSE(set.min_ev.has_value());
  CHECK_FALSE(set.speed_up.has_value());
}

TEST_CASE("UI font selection is captured per text and can return to the built-in face") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  CHECK(ruby(*engine, "UI.font.nil?") == "true");
  CHECK(ruby(*engine, "UI.font = :custom; UI.font") == "custom");
  ruby(*engine, "UI.text('Euro €', x: 10, y: 10)");
  ruby(*engine, "UI.font = nil; UI.text('Default', x: 10, y: 40)");
  const auto& hud = engine->script_context().hud;
  REQUIRE(hud.size() == 2);
  CHECK(hud[0].font == "custom");
  CHECK(hud[1].font.empty());
  CHECK_FALSE(engine->scripts().eval("UI.font = '../outside'", "invalid_font.rb").ok);
  CHECK_FALSE(engine->scripts().eval("UI.font = '/outside'", "invalid_font.rb").ok);
  CHECK(engine->script_context().ui_font.empty());
}

namespace {
class FontMeasurePresenter final : public Presenter {
 public:
  mutable std::string selected;
  mutable std::filesystem::path directory;
  SurfaceSize surface_size() const override { return {kScreenWidth, kScreenHeight}; }
  int text_width(const std::string&, int) const override { return 10; }
  int font_text_width(const std::string&, int, const std::string& font,
                      const std::filesystem::path& folder) const override {
    selected = font;
    directory = folder;
    return kMeasuredWidth;
  }
  void capture_cursor(bool) override {}
  void present(const FrameView&) override {}
  bool open() const override { return true; }
  float frame_seconds() const override { return config::kFixedDeltaSeconds; }
};
}  // namespace

TEST_CASE("text alignment measures the selected font from the game's own fonts folder") {
  FontMeasurePresenter presenter;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, &presenter);
  CHECK(ruby(*engine, "UI.font = :custom; UI.text_width('Wide')") == "40");
  CHECK(presenter.selected == "custom");
  CHECK(presenter.directory == std::filesystem::path(fixture("games/directed")) / "fonts");
  ruby(*engine, "UI.text('Centered', x: 100, y: 10, align: :center)");
  REQUIRE(engine->script_context().hud.size() == 1);
  CHECK(engine->script_context().hud[0].x == 80);
}

TEST_CASE("a clip is queued around what its block draws, inside the one around it") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  frame(*engine);
  ruby(*engine,
       "UI.clip(x: 10, y: 20, width: 300, height: 200) do\n"
       "  UI.clip(x: 200, y: 100, width: 300, height: 300) { UI.rect(x: 0, y: 0, width: 5, height: 5) }\n"
       "end");
  const std::vector<HudItem>& hud = engine->script_context().hud;
  REQUIRE(hud.size() == 5);
  CHECK(hud[0].kind == HudItem::Kind::Clip);
  CHECK(hud[1].kind == HudItem::Kind::Clip);
  CHECK((hud[1].x == 200 && hud[1].y == 100));
  CHECK((hud[1].width == 110 && hud[1].height == 120));  // only where both clips are
  CHECK(hud[2].kind == HudItem::Kind::Rect);
  CHECK(hud[3].kind == HudItem::Kind::Clip);  // back to the outer one
  CHECK(hud[3].width == 300);
  CHECK(hud[4].kind == HudItem::Kind::Unclip);
}

TEST_CASE("an anchored text and rectangle keep to their corner of the screen") {
  FontMeasurePresenter presenter;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, &presenter);
  frame(*engine);
  ruby(*engine, "UI.rect(anchor: :bottom_right, x: -10, y: -10, width: 100, height: 50)");
  ruby(*engine, "UI.text('Wide', anchor: :top_right, x: -20, y: 20)");
  const std::vector<HudItem>& hud = engine->script_context().hud;
  REQUIRE(hud.size() == 2);
  CHECK(hud[0].x == kScreenWidth - 110);
  CHECK(hud[0].y == kScreenHeight - 60);
  CHECK(hud[1].x == kScreenWidth - kMeasuredWidth - 20);  // its right edge 20 pixels from the screen's
  CHECK(hud[1].y == 20);
}

TEST_CASE("a text wraps at word boundaries to its width, measured as it is drawn") {
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene);
  frame(*engine);
  // (Measured as wide as a text is taken to be without a font: "one two" is 77 pixels at size 20.)
  CHECK(ruby(*engine, "UI.wrap('one two three', width: 80, size: 20)") == R"(["one two", "three"])");
  CHECK(ruby(*engine, "UI.text_height('one two three', width: 80, size: 20)") == "45");
  ruby(*engine, "UI.text('one two three', x: 0, y: 10, width: 80, size: 20)");
  const std::vector<HudItem>& hud = engine->script_context().hud;
  REQUIRE(hud.size() == 2);
  CHECK(hud[0].text == "one two");
  CHECK(hud[1].text == "three");
  CHECK(hud[1].y == 10 + 25);  // a line height (1.25 times the size) under the first
}
