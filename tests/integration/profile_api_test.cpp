#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace {
struct ProfileGame {
  double seconds = 0.0;
  int reads = 0;
  std::unique_ptr<Engine> engine;
  ProfileGame() {
    EngineOptions options;
    options.game_dir = test::fixture("games/sandbox");
    options.fixed_step = true;
    options.profile_time = [this] {
      ++reads;
      seconds += 0.001;
      return seconds;
    };
    auto created = Engine::create(options);
    REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
    engine = std::move(std::get<std::unique_ptr<Engine>>(created));
  }
  std::string ruby(const char* code) {
    const auto value = engine->scripts().eval(code, "profile_test.rb");
    REQUIRE_MESSAGE(value.ok, value.error);
    return RSTRING_PTR(mrb_obj_as_string(engine->scripts().vm().state(), value.value));
  }
  void frame() { engine->frame(config::kFixedDeltaSeconds); }
};
}  // namespace

TEST_CASE("profiling is off by default and inactive frames never read its clock") {
  ProfileGame game;
  CHECK(game.ruby("Debug.profiling?") == "false");
  CHECK(game.ruby("Debug.frame_stats[:samples]") == "0");
  game.frame();
  game.frame();
  CHECK(game.reads == 0);
  CHECK(game.ruby("Debug.stats_visible?") == "false");
}

TEST_CASE("frame statistics measure real work while simulation is paused") {
  ProfileGame game;
  game.ruby("Time.scale = 0; Debug.profiling = true");
  game.frame();
  game.frame();
  CHECK(game.ruby("Debug.frame_stats[:samples]") == "2");
  CHECK(game.ruby("Debug.frame_stats[:fps] > 0") == "true");
  CHECK(game.ruby("Debug.frame_stats[:latest][:frame_ms] > 0") == "true");
  CHECK(game.ruby("Debug.frame_stats[:latest][:update_ms] > 0") == "true");
  CHECK(game.ruby("Debug.frame_stats[:latest][:physics_ms] == 0") == "true");
  CHECK(game.engine->script_context().time.elapsed == 0.0);
  game.ruby("Debug.profiling = false");
  const int reads = game.reads;
  game.frame();
  CHECK(game.reads == reads);
  CHECK(game.ruby("Debug.frame_stats[:samples]") == "2");
}

TEST_CASE("statistics overlay is opt in and remains bounded across frames and scenes") {
  ProfileGame game;
  game.ruby("Debug.stats_visible = true");
  CHECK(game.ruby("Debug.profiling?") == "true");
  game.frame();
  REQUIRE_FALSE(game.engine->script_context().hud.empty());
  const auto items = game.engine->script_context().hud.size();
  game.frame();
  CHECK(game.engine->script_context().hud.size() == items);
  CHECK(game.engine->script_context().hud.back().text.find("scene:") == 0);
  game.ruby("Game.load_scene('level2')");
  game.frame();
  game.frame();
  CHECK(game.ruby("Debug.stats_visible?") == "true");
  CHECK(game.ruby("Debug.frame_stats[:samples]") == "4");
  game.ruby("Debug.stats_visible = false");
  game.frame();
  CHECK(game.engine->script_context().hud.empty());
  CHECK(game.ruby("Debug.profiling?") == "true");
  game.ruby("Debug.reset_frame_stats");
  CHECK(game.ruby("Debug.frame_stats[:samples]") == "0");
}
