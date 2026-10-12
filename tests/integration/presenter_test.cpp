// What the engine asks of whoever shows the game, and what it hands them every frame.
#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "input/touch_controls.hpp"
#include "core/config.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kScreenWidth = 640;
constexpr int kScreenHeight = 360;
constexpr int kLetterWidth = 7;  // what the recording presenter says every letter measures
constexpr float kFrameSeconds = 0.25f;
constexpr int kFramesUntilClosed = 3;
constexpr int kBurst = 5;
constexpr float kBlur = 0.4f;

// A copy of what a frame said, kept after the frame is gone.
struct SeenFrame {
  std::vector<HudItem> hud;
  int particles = 0;
  float motion_blur = 0.0f;
  std::vector<ControlCircle> controls;
  int scene_generation = 0;
  std::size_t meshes = 0;
  std::size_t objects = 0;
  std::filesystem::path images_dir;
};

// Shows nothing and remembers everything it was asked and handed.
class RecordingPresenter final : public Presenter {
 public:
  SurfaceSize surface_size() const override { return {kScreenWidth, kScreenHeight}; }
  int text_width(const std::string& text, int) const override { return static_cast<int>(text.size()) * kLetterWidth; }
  void capture_cursor(bool wanted) override { cursor_wanted.push_back(wanted); }
  void present(const FrameView& view) override {
    frames.push_back({view.hud, static_cast<int>(view.particles.particles().size()), view.motion_blur, view.controls,
                      view.scene_generation, view.scene_desc.meshes.size(), view.scene.alive_ids().size(),
                      view.images_dir});
  }
  bool open() const override {
    return frames_until_closed < 0 || static_cast<int>(frames.size()) < frames_until_closed;
  }
  float frame_seconds() const override { return kFrameSeconds; }

  std::vector<SeenFrame> frames;
  std::vector<bool> cursor_wanted;
  int frames_until_closed = -1;  // never, unless a test says after how many frames
};

// A touch screen with one finger on its A button.
class TouchScreen final : public InputSource {
 public:
  TouchScreen() { layout_.a.pressed = true; }
  bool is_key_down(Key) const override { return false; }
  const TouchLayout* touch_layout() const override { return shown_ ? &layout_ : nullptr; }
  void show_touch_controls(bool show) override { shown_ = show; }

 private:
  TouchLayout layout_;
  bool shown_ = true;
};

std::unique_ptr<Engine> sandbox(Presenter& presenter, InputSource* input = nullptr) {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.presenter = &presenter;
  options.input = input;
  options.fixed_step = true;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

void frame(Engine& engine) { engine.frame(config::kFixedDeltaSeconds); }

// A behaviour that draws a text and a rectangle over the game every frame.
constexpr const char* kPainter = R"(
  class Painter < Behaviour
    def update(_dt)
      UI.text("score", x: 10, y: 20, size: 24)
      UI.rect(x: 1, y: 2, width: 30, height: 40)
    end
  end
  Scene.find("Hero").add_component(Painter)
)";

}  // namespace

TEST_CASE("the presenter is handed each frame of the game: what scripts drew, particles, blur") {
  RecordingPresenter presenter;
  auto engine = sandbox(presenter);
  CHECK(presenter.frames.empty());  // nothing is shown before the game runs a frame
  ruby(*engine, kPainter);
  ruby(*engine, "Particles.burst(Vector3.new(0, 1, 0), count: " + std::to_string(kBurst) + ", life: 60.0)");
  ruby(*engine, "Camera.motion_blur = " + std::to_string(kBlur));
  frame(*engine);
  frame(*engine);
  REQUIRE(presenter.frames.size() == 2);
  const SeenFrame& seen = presenter.frames.back();
  REQUIRE(seen.hud.size() == 2);  // this frame's items only: last frame's were not kept
  CHECK(seen.hud[0].kind == HudItem::Kind::Text);
  CHECK(seen.hud[0].text == "score");
  CHECK(seen.hud[1].kind == HudItem::Kind::Rect);
  CHECK(seen.particles == kBurst);
  CHECK(seen.motion_blur == doctest::Approx(kBlur));
  CHECK(seen.controls.empty());  // no touch screen
  CHECK(seen.objects == engine->scene().alive_ids().size());
  CHECK(seen.meshes > 0);
  CHECK(seen.images_dir == std::filesystem::path(fixture("games/sandbox")) / config::kImagesDirName);
}

TEST_CASE("the game is as big as the presenter's surface, and texts as wide as it measures them") {
  RecordingPresenter presenter;
  auto engine = sandbox(presenter);
  CHECK(ruby(*engine, "UI.width") == std::to_string(kScreenWidth));  // from the start, before any frame
  CHECK(ruby(*engine, "UI.height") == std::to_string(kScreenHeight));
  CHECK(ruby(*engine, "UI.text_width('abcd', size: 30)") == std::to_string(4 * kLetterWidth));
}

TEST_CASE("the game asks for the pointer every frame; the presenter decides") {
  RecordingPresenter presenter;
  auto engine = sandbox(presenter);
  frame(*engine);
  ruby(*engine, "Input.mouse_locked = true");
  frame(*engine);
  CHECK(presenter.cursor_wanted == std::vector<bool>{false, true});
}

TEST_CASE("a touch screen's controls are part of the frame, unless the game hides them") {
  RecordingPresenter presenter;
  TouchScreen screen;
  auto engine = sandbox(presenter, &screen);
  frame(*engine);
  REQUIRE(presenter.frames.back().controls.size() == 5);  // the stick's ring and knob, A, B and Start
  CHECK(presenter.frames.back().controls[2].look == ControlCircle::Look::Pressed);
  ruby(*engine, "Input.touch_controls = false");
  frame(*engine);
  CHECK(presenter.frames.back().controls.empty());
}

TEST_CASE("a scene the game loads is a new generation of the frames, for whoever uploads its assets") {
  RecordingPresenter presenter;
  auto engine = sandbox(presenter);
  frame(*engine);
  CHECK(presenter.frames.back().scene_generation == kFirstSceneGeneration);
  ruby(*engine, "Game.load_scene('level2')");
  frame(*engine);  // the frame is shown, then the scene changes
  CHECK(presenter.frames.back().scene_generation == kFirstSceneGeneration);
  frame(*engine);
  CHECK(presenter.frames.back().scene_generation == kFirstSceneGeneration + 1);
}

TEST_CASE("a frame can be shown again without the game advancing") {
  RecordingPresenter presenter;
  auto engine = sandbox(presenter);
  ruby(*engine, kPainter);
  frame(*engine);
  const long long frame_number = engine->script_context().time.frame;
  engine->present();
  REQUIRE(presenter.frames.size() == 2);
  CHECK(presenter.frames.back().hud.size() == presenter.frames.front().hud.size());
  CHECK(engine->script_context().time.frame == frame_number);
}

TEST_CASE("the game runs for as long as the presenter has somewhere to show it") {
  RecordingPresenter presenter;
  presenter.frames_until_closed = kFramesUntilClosed;
  auto engine = sandbox(presenter);
  CHECK_FALSE(engine->finished());
  CHECK(engine->run() == 0);
  CHECK(presenter.frames.size() == kFramesUntilClosed);
  CHECK(engine->finished());
}

TEST_CASE("a game shown by nobody is as big as its settings say and steps at the fixed pace") {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  const GameSettings settings = read_game_settings(options.game_dir);
  CHECK(engine.presenter().surface_size().width == settings.window_width);
  CHECK(engine.presenter().surface_size().height == settings.window_height);
  CHECK(engine.presenter().frame_seconds() == doctest::Approx(config::kFixedDeltaSeconds));
  CHECK(engine.presenter().open());
  CHECK(engine.presenter().text_width("abcd", 10) == estimated_text_width("abcd", 10));
  CHECK(ruby(engine, "UI.text_width('abcd', size: 10)") == std::to_string(estimated_text_width("abcd", 10)));
}
