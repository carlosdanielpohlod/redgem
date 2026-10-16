// How long the game's own side of a frame takes (scripts, scene, physics; nothing is drawn), for
// whoever works on the engine's core: not a test, not shipped.
//
//   redgem_scripting_benchmark behaviours   thousands of objects, each with a behaviour that updates
//   redgem_scripting_benchmark churn        objects spawned every frame that destroy themselves a
//                                           second later, for minutes of game time
//   redgem_scripting_benchmark vectors      behaviours doing a frame's worth of Vector3 math each
//   redgem_scripting_benchmark vectors-in-place   the same math, changing vectors in place
//   redgem_scripting_benchmark rebuild FILE  how long making the scene of a glTF file's objects
//                                           takes (what the editor's preview does after an edit)
//
// It prints the average time of a frame and, for churn, how it changes as the game goes on.

#include <mruby.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "scene/gltf_loader.hpp"

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
constexpr int kRebuilds = 200;
constexpr int kMovers = 1000;

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

# Steering math of the kind a game does every frame: a few sums, scalings, a cross product.
class Mover < Behaviour
  def start
    @velocity = Vector3.new(1, 0, 0.5)
    @lift = Vector3.new(0, 0.1, 0)
  end

  def update(dt)
    place = transform.position
    8.times { place = ((place + @velocity * dt) - @lift).normalized * 2.0 + place.cross(Vector3.up) * 0.01 }
    transform.position = place
  end
end

# The same steps, changing vectors in place: one new vector a step instead of seven.
class InPlaceMover < Behaviour
  def start
    @velocity = Vector3.new(1, 0, 0.5)
    @lift = Vector3.new(0, 0.1, 0)
    @up = Vector3.up
    @step = Vector3.new
  end

  def update(dt)
    place = transform.position
    @step.set(@velocity.x, @velocity.y, @velocity.z).scale!(dt)
    8.times do
      spin = place.cross(@up).scale!(0.01)
      place.add!(@step).sub!(@lift).normalize!.scale!(2.0).add!(spin)
    end
    transform.position = place
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

int vectors(Engine& engine, const char* mover) {
  if (!ruby(engine, spawn_code(kMovers, mover))) return 1;
  engine.frame(redgem::config::kFixedDeltaSeconds);
  const double seconds = seconds_per_frame(engine, kBehaviourFrames);
  std::printf("%d %s behaviours doing Vector3 math: %.1f us a frame\n", kMovers, mover,
              seconds * kMicrosecondsPerSecond);
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

int rebuild(const std::string& file) {
  const redgem::LoadResult loaded = redgem::load_gltf_scene(file);
  if (!loaded.ok) {
    std::fprintf(stderr, "cannot read %s: %s\n", file.c_str(), loaded.error.c_str());
    return 1;
  }
  std::size_t objects = 0;
  const Clock::time_point start = Clock::now();
  for (int i = 0; i < kRebuilds; ++i) objects = redgem::Scene::from_desc(loaded.scene).size();
  const double seconds = std::chrono::duration<double>(Clock::now() - start).count() / kRebuilds;
  std::printf("%zu objects: %.1f us a rebuild\n", objects, seconds * kMicrosecondsPerSecond);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "behaviours";
  if (mode == "rebuild" && argc > 2) return rebuild(argv[2]);
  std::unique_ptr<Engine> engine = sandbox(argc > 2 ? argv[2] : REDGEM_BENCHMARK_GAME);
  if (!engine || !ruby(*engine, kBehaviours)) return 1;
  if (mode == "churn") return churn(*engine);
  if (mode == "vectors") return vectors(*engine, "Mover");
  if (mode == "vectors-in-place") return vectors(*engine, "InPlaceMover");
  return behaviours(*engine);
}
