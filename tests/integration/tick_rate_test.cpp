// game.json's "tick_hz": how many fixed steps a game takes a second. Physics, fixed_update,
// Time.fixed_delta and a tool's frame all follow it, at the default 60 and at 128.

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "app/game_settings.hpp"
#include "app/presenter.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "physics/physics_config.hpp"
#include "physics_game.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr int kFastTick = 128;
constexpr float kDropHeight = 20.0f;  // meters: high enough not to land within a second
// What a body falls in one second, g t^2 / 2, give or take (a share of it) what a step's length
// and the body's damping change of it.
constexpr float kFallInOneSecond = -config::kGravityY / 2.0f;
constexpr float kFallTolerance = 0.04f;
constexpr int kRealFramesPerSecond = 60;  // frames of a window at 60 fps, on the real clock
constexpr int kMissedStepAtMost = 1;      // what the real clock's rounding may leave for the next frame

// Counts the fixed steps it is given, and keeps the last one's length and the last frame's.
constexpr const char* kStepCounter = R"(
class StepCounter < Behaviour
  def start
    $steps = 0
  end

  def fixed_update(dt)
    $steps += 1
    $step_dt = dt
  end

  def update(dt)
    $frame_dt = dt
  end
end
)";

std::string game_ticking_at(int tick_hz) { return "{\"tick_hz\": " + std::to_string(tick_hz) + "}"; }

// A game ticking at `tick_hz` with a step counter and a crate up in the air.
struct TickGame {
  TempGame game;
  SceneDesc scene;
  std::unique_ptr<Engine> engine;

  explicit TickGame(int tick_hz, bool fixed_step = true) : game(game_ticking_at(tick_hz)) {
    std::string error;
    std::filesystem::create_directories(game.dir / config::kScriptsDirName);
    const auto script = game.dir / config::kScriptsDirName / "step_counter.rb";
    REQUIRE_MESSAGE(write_text_file(script, kStepCounter, error), error);
    scene = scene_with({node("Counter", {}, {1, 1, 1}, {{"script", std::string("step_counter.rb")}}),
                        node("Crate", {0, kDropHeight, 0}, {1, 1, 1}, moving("dynamic"))});
    engine = fixed_step ? engine_for(game, scene) : real_clock_engine();
  }

  std::unique_ptr<Engine> real_clock_engine() {
    EngineOptions options;
    options.game_dir = game.dir;
    options.scene = &scene;
    auto created = Engine::create(options);
    if (auto* failed = std::get_if<std::string>(&created)) FAIL(*failed);
    return std::move(std::get<std::unique_ptr<Engine>>(created));
  }

  float number(const std::string& code) { return std::stof(ruby(*engine, code)); }
  float crate_height() { return number("Scene.find('Crate').transform.position.y"); }
};

GameManifest manifest_of(const std::string& game_json) {
  const TempGame game(game_json);
  return read_game_manifest(game.dir);
}

}  // namespace

TEST_CASE("a game ticks at game.json's tick_hz, else at 60") {
  CHECK(manifest_of("{}").settings.tick_hz == config::kDefaultTickHz);
  CHECK(manifest_of("{}").tick_hz_error.empty());
  for (const int asked : {config::kMinTickHz, config::kDefaultTickHz, kFastTick, config::kMaxTickHz}) {
    CAPTURE(asked);
    CHECK(manifest_of(game_ticking_at(asked)).settings.tick_hz == asked);
  }
}

TEST_CASE("a tick_hz the engine cannot take is refused, and the game ticks at 60") {
  for (const char* unusable : {"29", "129", "0", "-60", "64.5", "\"fast\"", "null"}) {
    CAPTURE(unusable);
    const GameManifest manifest = manifest_of(std::string("{\"tick_hz\": ") + unusable + "}");
    CHECK(manifest.settings.tick_hz == config::kDefaultTickHz);
    CHECK(manifest.tick_hz_error.find("tick_hz must be a whole number from 30 to 128") != std::string::npos);
  }
}

TEST_CASE("a frame catches up on as much game time at any tick") {
  CHECK(config::max_steps_per_frame(config::kDefaultTickHz) == config::kMaxFixedStepsPerFrame);
  const float caught_up = static_cast<float>(config::max_steps_per_frame(kFastTick)) * config::tick_seconds(kFastTick);
  CHECK(caught_up >= static_cast<float>(config::kMaxFixedStepsPerFrame) * config::kDefaultFixedDeltaSeconds);
}

TEST_CASE("Time.fixed_delta, fixed_update, the frame of a fixed-step run and physics follow the tick") {
  for (const int tick : {config::kDefaultTickHz, kFastTick}) {
    CAPTURE(tick);
    TickGame played(tick);
    const float step = config::tick_seconds(tick);
    CHECK(played.engine->fixed_delta() == doctest::Approx(step));
    CHECK(played.engine->presenter().frame_seconds() == doctest::Approx(step));
    frames(*played.engine, tick);  // one second of the game
    CHECK(played.number("Time.fixed_delta") == doctest::Approx(step));
    CHECK(played.number("$step_dt") == doctest::Approx(step));
    CHECK(played.number("$frame_dt") == doctest::Approx(step));
    CHECK(ruby(*played.engine, "$steps") == std::to_string(tick));
    CHECK(played.number("Time.elapsed") == doctest::Approx(1.0f));
    CHECK(kDropHeight - played.crate_height() == doctest::Approx(kFallInOneSecond).epsilon(kFallTolerance));
  }
}

TEST_CASE("on the real clock, a game at 128 Hz takes 128 steps in a second of 60 fps frames") {
  TickGame played(kFastTick, false);
  for (int frame = 0; frame < kRealFramesPerSecond; ++frame) {
    played.engine->frame(config::tick_seconds(kRealFramesPerSecond));
  }
  const int steps = std::stoi(ruby(*played.engine, "$steps"));
  CHECK(steps >= kFastTick - kMissedStepAtMost);
  CHECK(steps <= kFastTick);
  CHECK(played.number("$step_dt") == doctest::Approx(config::tick_seconds(kFastTick)));
  CHECK(played.number("$frame_dt") == doctest::Approx(config::tick_seconds(kRealFramesPerSecond)));
}
