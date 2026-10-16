// How long the game's own side of a frame takes (scripts, scene, physics; nothing is drawn), for
// whoever works on the engine's core: not a test, not shipped.
//
//   redgem_scripting_benchmark behaviours   thousands of objects, each with a behaviour that updates
//   redgem_scripting_benchmark churn        objects spawned every frame that destroy themselves a
//                                           second later, for minutes of game time
//
// It prints the average time of a frame and, for churn, how it changes as the game goes on.

#include <mruby.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

#include "app/engine.hpp"
#include "core/config.hpp"

namespace {

using redgem::Engine;
using Clock = std::chrono::steady_clock;

constexpr int kBehaviourObjects = 4000;
constexpr int kBehaviourFrames = 600;     // ten seconds of game
constexpr int kChurnSpawnsPerFrame = 20;  // twelve hundred objects a second
constexpr int kChurnMinutes = 2;
constexpr int kFramesPerMinute = 60 * 60;  // at the fixed 60 Hz step
constexpr int kChurnReports = 5;           // how many times the frame time is printed
constexpr double kMicrosecondsPerSecond = 1e6;

// A behaviour that does a little work every frame, and one that leaves a second after it started.
constexpr const char* kBehaviours = R"(
class Ticker < Behaviour
  def update(dt) = @elapsed = (@elapsed || 0.0) + dt
end

class Fleeting < Behaviour
  def update(dt)
    @left = (@left || 1.0) - dt
    destroy if @left <= 0
  end
end

class Burst < Behaviour
  def update(_dt) = prop(:count).times { Scene.spawn('Bullet', physics: false).add_component(Fleeting) }
end
)";

std::unique_ptr<Engine> sandbox(const std::string& game_dir) {
  redgem::EngineOptions options;
  options.game_dir = game_dir;
  options.fixed_step = true;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) {
    std::fprintf(stderr, "cannot start the game: %s\n", error->c_str());
    return nullptr;
  }
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

bool ruby(Engine& engine, const std::string& code) {
  const redgem::RubyResult result = engine.scripts().eval(code, "benchmark.rb");
  if (!result.ok) std::fprintf(stderr, "%s\n", result.error.c_str());
  return result.ok;
}

std::string spawn_code(int count, const char* behaviour) {
  return std::to_string(count) + ".times { Scene.spawn('Bullet', physics: false).add_component(" + behaviour + ") }";
}

// Seconds each of `frames` frames took, on average.
double seconds_per_frame(Engine& engine, int frames) {
  const Clock::time_point start = Clock::now();
  for (int i = 0; i < frames; ++i) engine.frame(redgem::config::kFixedDeltaSeconds);
  return std::chrono::duration<double>(Clock::now() - start).count() / frames;
}

// How many objects Ruby holds right after a full collection: what a leak would make grow.
std::string ruby_objects(Engine& engine) {
  const redgem::RubyResult counted = engine.scripts().eval(
      "GC.start; ObjectSpace.count_objects[:TOTAL] - ObjectSpace.count_objects[:FREE]", "benchmark.rb");
  if (!counted.ok) return "?";
  return std::to_string(mrb_integer(counted.value));
}

int behaviours(Engine& engine) {
  if (!ruby(engine, spawn_code(kBehaviourObjects, "Ticker"))) return 1;
  engine.frame(redgem::config::kFixedDeltaSeconds);  // their starts run once, outside the measure
  const double seconds = seconds_per_frame(engine, kBehaviourFrames);
  std::printf("%d updating behaviours: %.1f us a frame\n", kBehaviourObjects, seconds * kMicrosecondsPerSecond);
  return 0;
}

int churn(Engine& engine) {
  const std::string burst =
      "Scene.spawn('Bullet', physics: false).add_component(Burst, count: " + std::to_string(kChurnSpawnsPerFrame) + ")";
  if (!ruby(engine, burst)) return 1;
  const int frames_per_report = kChurnMinutes * kFramesPerMinute / kChurnReports;
  for (int report = 1; report <= kChurnReports; ++report) {
    const double seconds = seconds_per_frame(engine, frames_per_report);
    const double minutes = static_cast<double>(report * frames_per_report) / kFramesPerMinute;
    std::printf("after %.1f min: %.1f us a frame, %zu object slots, %s live Ruby objects\n", minutes,
                seconds * kMicrosecondsPerSecond, engine.scene().size(), ruby_objects(engine).c_str());
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "behaviours";
  std::unique_ptr<Engine> engine = sandbox(argc > 2 ? argv[2] : REDGEM_BENCHMARK_GAME);
  if (!engine || !ruby(*engine, kBehaviours)) return 1;
  if (mode == "churn") return churn(*engine);
  return behaviours(*engine);
}
