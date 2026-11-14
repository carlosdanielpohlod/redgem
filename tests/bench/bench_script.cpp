// The script VM's side of a frame: many objects, each with a behaviour whose update runs every
// frame, in an engine nobody shows (the sandbox fixture game; nothing is drawn).

#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "bench/bench_case.hpp"
#include "core/config.hpp"

namespace redgem::bench {

namespace {

constexpr int kBehaviours = 1000;
constexpr int kFramesPerSample = 10;

// A behaviour that does a little work every frame.
constexpr const char* kTicker = R"(
class BenchTicker < Behaviour
  def update(dt)
    @elapsed = (@elapsed || 0.0) + dt
    @turns = (@turns || 0) + 1
  end
end
)";

std::shared_ptr<Engine> sandbox() {
  EngineOptions options;
  options.game_dir = REDGEM_BENCH_SANDBOX;
  options.fixed_step = true;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) require(false, "starting the sandbox: " + *error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

void ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "bench.rb");
  require(result.ok, "Ruby (" + result.error + ")");
}

std::string spawn_code() {
  return std::to_string(kBehaviours) + ".times { Scene.spawn('Bullet', physics: false).add_component(BenchTicker) }";
}

}  // namespace

BenchCase script_case() {
  const std::string what = "one frame of " + std::to_string(kBehaviours) + " objects whose behaviour updates";
  return {"script.update_behaviours", what, [] {
            std::shared_ptr<Engine> engine = sandbox();
            ruby(*engine, kTicker);
            ruby(*engine, spawn_code());
            engine->frame(config::kFixedDeltaSeconds);  // their starts run once, outside the measure
            return Sampler([engine] {
              return microseconds_each(kFramesPerSample, [&engine] { engine->frame(config::kFixedDeltaSeconds); });
            });
          }};
}

}  // namespace redgem::bench
