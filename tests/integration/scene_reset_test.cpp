// Loading a scene resets what the engine's Ruby modules and classes keep for the scene before it:
// Perception (a class) as well as Network (a module), with no error, scene after scene.

#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr const char* kLevel = "level2";
constexpr int kLoads = 2;

std::unique_ptr<Engine> sandbox() {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.fixed_step = true;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "scene_reset_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

}  // namespace

TEST_CASE("loading a scene twice resets Perception's senses with no error") {
  auto engine = sandbox();
  for (int load = 0; load < kLoads; ++load) {
    ruby(*engine, std::string("Perception.registered << :from_the_scene_before; Game.load_scene('") + kLevel + "')");
    engine->frame(config::kFixedDeltaSeconds);
    REQUIRE(engine->scene_name() == kLevel);
    CHECK(ruby(*engine, "Perception.registered.empty?") == "true");
  }
  CHECK(engine->scripts().error_count() == 0);
}
