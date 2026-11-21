// A game drawn more often than it ticks draws the bodies the physics moves between their last two
// steps (app/render_poses.hpp); its scripts still see where the steps left them.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "app/presenter.hpp"
#include "core/config.hpp"
#include "physics_game.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr int kDrawnHz = 240;               // frames a second: four to a step of the default tick
constexpr float kSpeed = 6.0f;              // meters a second, along x
constexpr float kStart = 0.0f;
constexpr int kWarmUpFrames = 8;            // a step or two before the first poses are both known
constexpr int kMeasuredFrames = 120;        // half a second of frames
constexpr float kEvenShare = 0.1f;          // how far a frame's step may be from the even one, as a share of it
constexpr float kTeleportX = 50.0f;
constexpr float kCarriedOffset = 2.0f;      // meters above the crate its rider sits
constexpr float kNear = 1e-4f;
constexpr float kMillimetersPerMeter = 1000.0f;

constexpr const char* kSlider = R"(
class Slider < Behaviour
  def start
    rigidbody.gravity_scale = 0
    rigidbody.velocity = Vector3.new(6, 0, 0)
    $seen = []
  end

  def update(_dt)
    $seen << transform.position.x
  end
end
)";

// Keeps, every frame, where the scene being drawn has the crate and what rides on it.
class PosePresenter final : public Presenter {
 public:
  SurfaceSize surface_size() const override { return {config::kWindowWidth, config::kWindowHeight}; }
  int text_width(const std::string& text, int) const override { return static_cast<int>(text.size()); }
  void capture_cursor(bool) override {}
  bool open() const override { return true; }
  float frame_seconds() const override { return config::tick_seconds(kDrawnHz); }
  void present(const FrameView& view) override {
    crate.push_back(view.scene.world_transform(view.scene.find("Crate")).position);
    rider.push_back(view.scene.world_transform(view.scene.find("Rider")).position);
  }
  std::vector<Vec3> crate;
  std::vector<Vec3> rider;
};

// A crate sliding along x with nothing to stop it, a rider on it, drawn at kDrawnHz on the clock.
struct SlidingGame {
  TempGame game{"{}"};
  SceneDesc scene;
  PosePresenter screen;
  std::unique_ptr<Engine> engine;

  explicit SlidingGame(bool fixed_step = false) {
    std::string error;
    std::filesystem::create_directories(game.dir / config::kScriptsDirName);
    REQUIRE_MESSAGE(write_text_file(game.dir / config::kScriptsDirName / "slider.rb", kSlider, error), error);
    PropertyMap crate = moving("dynamic");
    crate["script"] = std::string("slider.rb");
    scene = scene_with({node("Crate", {kStart, 1, 0}, {1, 1, 1}, crate),
                        node("Rider", {0, kCarriedOffset, 0}, {1, 1, 1}, {})});
    const int crate_at = static_cast<int>(scene.nodes.size()) - 2;  // (the rider is last, the crate before it)
    scene.nodes[crate_at].children.push_back(crate_at + 1);
    scene.nodes[crate_at + 1].parent = crate_at;
    EngineOptions options;
    options.game_dir = game.dir;
    options.scene = &scene;
    options.presenter = &screen;
    options.fixed_step = fixed_step;
    auto created = Engine::create(options);
    if (auto* failed = std::get_if<std::string>(&created)) FAIL(*failed);
    engine = std::move(std::get<std::unique_ptr<Engine>>(created));
  }

  void play(int frames) {
    for (int i = 0; i < frames; ++i) engine->frame(config::tick_seconds(kDrawnHz));
  }
};

// How far the crate went from each frame drawn to the next, along x, past the warm-up.
std::vector<float> drawn_steps(const std::vector<Vec3>& drawn) {
  std::vector<float> steps;
  for (std::size_t i = kWarmUpFrames + 1; i < drawn.size(); ++i) steps.push_back(drawn[i].x - drawn[i - 1].x);
  return steps;
}

}  // namespace

TEST_CASE("at 240 frames a second a sliding body is drawn a quarter of a step further each frame") {
  SlidingGame sliding;
  sliding.play(kWarmUpFrames + kMeasuredFrames);
  const float even = kSpeed / static_cast<float>(kDrawnHz);
  for (const float step : drawn_steps(sliding.screen.crate)) {
    CHECK(step == doctest::Approx(even).epsilon(kEvenShare));
  }
}

TEST_CASE("what rides on an interpolated body is drawn with it") {
  SlidingGame sliding;
  sliding.play(kWarmUpFrames + kMeasuredFrames);
  for (std::size_t i = 0; i < sliding.screen.crate.size(); ++i) {
    CHECK(sliding.screen.rider[i].x == doctest::Approx(sliding.screen.crate[i].x).epsilon(kNear));
    CHECK(sliding.screen.rider[i].y == doctest::Approx(sliding.screen.crate[i].y + kCarriedOffset).epsilon(kNear));
  }
}

TEST_CASE("scripts see the body where the steps left it, a step's worth at a time") {
  SlidingGame sliding;
  sliding.play(kWarmUpFrames + kMeasuredFrames);
  // In millimeters: every frame's move is a whole number of steps, most often none.
  const long step = std::lround(kSpeed * config::kDefaultFixedDeltaSeconds * kMillimetersPerMeter);
  const std::string moves = "$seen.each_cons(2).map { |a, b| ((b - a) * " + std::to_string(kMillimetersPerMeter) +
                            ").round }";
  CHECK(ruby(*sliding.engine, moves + ".all? { |mm| mm % " + std::to_string(step) + " == 0 }") == "true");
  CHECK(ruby(*sliding.engine, moves + ".count(0) > " + moves + ".size / 2") == "true");
  // After a frame, the scene holds the step's pose again.
  const float x = std::stof(ruby(*sliding.engine, "Scene.find('Crate').transform.position.x"));
  CHECK(x == doctest::Approx(std::stof(ruby(*sliding.engine, "$seen.last"))));
}

TEST_CASE("a body a script moved since its last step is drawn where the script put it") {
  SlidingGame sliding;
  sliding.play(kWarmUpFrames);
  ruby(*sliding.engine, "Scene.find('Crate').transform.position = Vector3.new(" + std::to_string(kTeleportX) + ", 1, 0)");
  sliding.engine->present();
  CHECK(sliding.screen.crate.back().x == doctest::Approx(kTeleportX));
}

TEST_CASE("a game that takes a whole step a frame draws the step's pose") {
  SlidingGame sliding(true);
  sliding.play(kWarmUpFrames);
  const float x = std::stof(ruby(*sliding.engine, "Scene.find('Crate').transform.position.x"));
  CHECK(sliding.screen.crate.back().x == doctest::Approx(x));
}
