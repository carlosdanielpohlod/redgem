// A game drawn more often than it ticks draws the bodies the physics moves between their last two
// steps (app/render_poses.hpp); its scripts still see where the steps left them.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "app/app_config.hpp"
#include "app/presenter.hpp"
#include "core/config.hpp"
#include "physics_game.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr int kDrawnHz = 240;   // frames a second: four to a step of the default tick
constexpr float kSpeed = 6.0f;  // meters a second, along x
constexpr float kStart = 0.0f;
constexpr int kWarmUpFrames = 8;      // a step or two before the first poses are both known
constexpr int kMeasuredFrames = 120;  // half a second of frames
constexpr float kEvenShare = 0.1f;    // how far a frame's step may be from the even one, as a share of it
constexpr float kTeleportX = 50.0f;
constexpr int kOverlappingThreads = 2;  // physics threads: the frame's last step overlaps its drawing
constexpr float kCarriedOffset = 2.0f;  // meters above the crate its rider sits
constexpr float kNear = 1e-4f;
constexpr float kMillimetersPerMeter = 1000.0f;
// Millimeters a frame's move may be off a whole number of steps: the body slows a little, as
// Jolt damps every body's speed by default (0.05 a second).
constexpr long kWholeStepsOff = 5;

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

// How a sliding game runs.
struct Sliding {
  bool fixed_step = false;             // a whole step a frame, as the tools take
  std::optional<int> physics_threads;  // more than one overlaps each frame's last step with its drawing
  bool rider_slides = false;           // the rider a sliding body of its own, listed before the crate
};

// A sliding body: the crate's own, or a rider's that slides with it.
PropertyMap slider() {
  PropertyMap props = moving("dynamic");
  props["script"] = std::string("slider.rb");
  return props;
}

// The crate, and the rider that hangs from it, in the order `how` lists them.
SceneDesc sliding_scene(const Sliding& how) {
  NodeDesc crate = node("Crate", {kStart, 1, 0}, {1, 1, 1}, slider());
  NodeDesc rider = node("Rider", {0, kCarriedOffset, 0}, {1, 1, 1}, how.rider_slides ? slider() : PropertyMap{});
  SceneDesc scene = how.rider_slides ? scene_with({rider, crate}) : scene_with({crate, rider});
  const int last = static_cast<int>(scene.nodes.size()) - 1;
  const int crate_at = how.rider_slides ? last : last - 1;
  const int rider_at = how.rider_slides ? last - 1 : last;
  scene.nodes[crate_at].children.push_back(rider_at);
  scene.nodes[rider_at].parent = crate_at;
  return scene;
}

// A crate sliding along x with nothing to stop it, a rider on it, drawn at kDrawnHz on the clock.
struct SlidingGame {
  TempGame game{"{}"};
  SceneDesc scene;
  PosePresenter screen;
  std::unique_ptr<Engine> engine;

  explicit SlidingGame(const Sliding& how = {}) {
    std::string error;
    std::filesystem::create_directories(game.dir / config::kScriptsDirName);
    REQUIRE_MESSAGE(write_text_file(game.dir / config::kScriptsDirName / "slider.rb", kSlider, error), error);
    scene = sliding_scene(how);
    EngineOptions options;
    options.game_dir = game.dir;
    options.scene = &scene;
    options.presenter = &screen;
    options.fixed_step = how.fixed_step;
    options.physics_threads = how.physics_threads;
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

// Where the scripts saw the crate each frame (where the steps left it), as x alone.
std::vector<Vec3> seen_by_scripts(Engine& engine) {
  std::istringstream text(ruby(engine, "$seen.join(' ')"));
  std::vector<Vec3> seen;
  for (float x = 0.0f; text >> x;) seen.push_back({x, 0.0f, 0.0f});
  return seen;
}

// The least, the most and the spread (standard deviation) of a frame's step, in millimeters.
std::string spread(const std::vector<float>& steps) {
  const float count = static_cast<float>(steps.size());
  const float mean = std::accumulate(steps.begin(), steps.end(), 0.0f) / count;
  float squares = 0.0f;
  for (const float step : steps) squares += (step - mean) * (step - mean);
  const auto [least, most] = std::minmax_element(steps.begin(), steps.end());
  std::ostringstream text;
  text << "min " << *least * kMillimetersPerMeter << " mm, max " << *most * kMillimetersPerMeter << " mm, sd "
       << std::sqrt(squares / count) * kMillimetersPerMeter << " mm";
  return text.str();
}

}  // namespace

TEST_CASE("at 240 frames a second a sliding body is drawn a quarter of a step further each frame") {
  SlidingGame sliding;
  sliding.play(kWarmUpFrames + kMeasuredFrames);
  const float even = kSpeed / static_cast<float>(kDrawnHz);
  for (const float step : drawn_steps(sliding.screen.crate)) {
    CHECK(step == doctest::Approx(even).epsilon(kEvenShare));
  }
  // The measurement for the record: the drawn steps, and the physics' own, frame by frame.
  MESSAGE("PROBE drawn: " << spread(drawn_steps(sliding.screen.crate))
                          << "; physics: " << spread(drawn_steps(seen_by_scripts(*sliding.engine))));
}

TEST_CASE("with its last step on the physics thread, a sliding body is still drawn evenly") {
  Sliding overlapped;
  overlapped.physics_threads = kOverlappingThreads;
  SlidingGame sliding(overlapped);
  sliding.play(kWarmUpFrames + kMeasuredFrames);
  const float even = kSpeed / static_cast<float>(kDrawnHz);
  for (const float step : drawn_steps(sliding.screen.crate)) {
    CHECK(step == doctest::Approx(even).epsilon(kEvenShare));
  }
}

TEST_CASE("a sliding body that hangs from another, listed before it, is drawn evenly too") {
  Sliding hanging;
  hanging.rider_slides = true;
  SlidingGame sliding(hanging);
  sliding.play(kWarmUpFrames + kMeasuredFrames);
  const float even = kSpeed / static_cast<float>(kDrawnHz);
  for (const float step : drawn_steps(sliding.screen.rider)) {
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
  // In millimeters: every frame's move is about a whole number of steps, most often none.
  const long step = std::lround(kSpeed * config::kDefaultFixedDeltaSeconds * kMillimetersPerMeter);
  const std::string moves =
      "$seen.each_cons(2).map { |a, b| ((b - a) * " + std::to_string(kMillimetersPerMeter) + ").round }";
  const std::string steps = std::to_string(step);
  const std::string off_whole = "(mm - (mm.to_f / " + steps + ").round * " + steps + ").abs";
  CHECK(ruby(*sliding.engine, moves + ".all? { |mm| " + off_whole + " <= " + std::to_string(kWholeStepsOff) + " }") ==
        "true");
  CHECK(ruby(*sliding.engine, moves + ".count(0) > " + moves + ".size / 2") == "true");
  // After a frame, the scene holds the step's pose again.
  const float x = std::stof(ruby(*sliding.engine, "Scene.find('Crate').transform.position.x"));
  CHECK(x == doctest::Approx(std::stof(ruby(*sliding.engine, "$seen.last"))));
}

TEST_CASE("a body a script moved since its last step is drawn where the script put it") {
  SlidingGame sliding;
  sliding.play(kWarmUpFrames);
  const std::string teleported = "Vector3.new(" + std::to_string(kTeleportX) + ", 1, 0)";
  ruby(*sliding.engine, "Scene.find('Crate').transform.position = " + teleported);
  sliding.engine->present();
  CHECK(sliding.screen.crate.back().x == doctest::Approx(kTeleportX));
}

TEST_CASE("a game that takes a whole step a frame draws the step's pose") {
  Sliding whole_steps;
  whole_steps.fixed_step = true;
  SlidingGame sliding(whole_steps);
  sliding.play(kWarmUpFrames);
  const float x = std::stof(ruby(*sliding.engine, "Scene.find('Crate').transform.position.x"));
  CHECK(sliding.screen.crate.back().x == doctest::Approx(x));
}

TEST_CASE("render_position is where the frame draws an object; position, where the physics has it") {
  SlidingGame sliding;
  sliding.play(kWarmUpFrames + 1);  // (a frame between two steps: the two differ)
  ruby(*sliding.engine, "$crate = Scene.find('Crate'); $rider = Scene.find('Rider')");
  // What update read last frame is what that frame drew.
  sliding.engine->frame(config::tick_seconds(kDrawnHz));
  const float drawn = std::stof(ruby(*sliding.engine, "$crate.transform.render_position.x"));
  const float stepped = std::stof(ruby(*sliding.engine, "$crate.transform.position.x"));
  CHECK(drawn == doctest::Approx(sliding.screen.crate.back().x));
  CHECK(stepped != doctest::Approx(drawn));
  const float rider = std::stof(ruby(*sliding.engine, "$rider.transform.render_position.x"));
  CHECK(rider == doctest::Approx(sliding.screen.rider.back().x));
}

TEST_CASE("a game that takes a whole step a frame renders where the physics has it") {
  Sliding whole_steps;
  whole_steps.fixed_step = true;
  SlidingGame sliding(whole_steps);
  sliding.play(kWarmUpFrames);
  CHECK(ruby(*sliding.engine, "t = Scene.find('Crate').transform; t.render_position == t.position") == "true");
}
