// What the editor reads of the NPC parts a running game's scripts gave an object
// (ScriptHost::npc_settings), and that the defaults it draws while editing are the parts' own.

#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "script/npc_settings.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr float kTolerance = 1e-4f;

std::unique_ptr<Engine> sandbox() {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.fixed_step = true;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  auto engine = std::move(std::get<std::unique_ptr<Engine>>(created));
  engine->frame(config::kDefaultFixedDeltaSeconds);
  return engine;
}

void ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "npc_settings_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
}

float ruby_number(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "npc_settings_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return static_cast<float>(mrb_as_float(engine.scripts().vm().state(), result.value));
}

// An object of the scene, and how its scripts find it.
struct Npc {
  ObjectId id;
  std::string find;
};

Npc first_object(Engine& engine) {
  const ObjectId id = engine.scene().alive_ids().front();
  return {id, "Scene.find('" + engine.scene().at(id).name + "')"};
}

}  // namespace

TEST_CASE("an object without NPC parts has no NPC settings") {
  auto engine = sandbox();
  const NpcSettings settings = engine->scripts().npc_settings(first_object(*engine).id);
  CHECK_FALSE(settings.interactable);
  CHECK_FALSE(settings.perception);
  CHECK(settings.schedule.empty());
  CHECK_FALSE(settings.route);
}

TEST_CASE("the editor reads the NPC parts the scripts gave an object") {
  auto engine = sandbox();
  const Npc npc = first_object(*engine);
  ruby(*engine, npc.find + ".add_component(Interactable, range: 3, prompt: 'npc.talk')");
  ruby(*engine, npc.find + ".add_component(Perception, sight: 8.0, fov: 90.0, eyes: 1.5, hearing: 6.0)");
  ruby(*engine, npc.find + ".add_component(Schedule, plan: '06:00 bake, 20:00 sleep')");
  const NpcSettings settings = engine->scripts().npc_settings(npc.id);
  REQUIRE(settings.interactable);
  CHECK(settings.interactable->range == doctest::Approx(3.0f));
  CHECK(settings.interactable->prompt == std::optional<std::string>("npc.talk"));
  REQUIRE(settings.perception);
  CHECK(settings.perception->sight == doctest::Approx(8.0f));
  CHECK(settings.perception->fov == doctest::Approx(90.0f));
  CHECK(settings.perception->eyes == doctest::Approx(1.5f));
  CHECK(settings.perception->hearing == doctest::Approx(6.0f));
  CHECK(settings.schedule == std::vector<std::string>{"06:00 bake", "20:00 sleep"});
}

TEST_CASE("the editor reads where a NavAgent was sent and the corners ahead of it") {
  auto engine = sandbox();
  const Npc npc = first_object(*engine);
  ruby(*engine,
       "agent = " + npc.find +
           ".add_component(NavAgent); goal = Vector3.new(4, 0, 2); "
           "agent.instance_variable_set(:@destination, goal); agent.instance_variable_set(:@waypoints, [goal])");
  const NpcSettings settings = engine->scripts().npc_settings(npc.id);
  REQUIRE(settings.route);
  CHECK(settings.route->destination.x == doctest::Approx(4.0f));
  CHECK(settings.route->destination.z == doctest::Approx(2.0f));
  REQUIRE(settings.route->waypoints.size() == 1);
}

TEST_CASE("a part whose settings are wrong gives the editor nothing, with no error of the game's") {
  auto engine = sandbox();
  const Npc npc = first_object(*engine);
  ruby(*engine, npc.find + ".add_component(Interactable, range: -1)");
  const int errors_before = engine->scripts().error_count();
  CHECK_FALSE(engine->scripts().npc_settings(npc.id).interactable);
  CHECK(engine->scripts().error_count() == errors_before);
}

TEST_CASE("the defaults the editor draws while editing are the NPC parts' own") {
  auto engine = sandbox();
  CHECK(kInteractableRange == doctest::Approx(ruby_number(*engine, "Interactable::RANGE")).epsilon(kTolerance));
  CHECK(kPerceptionSight == doctest::Approx(ruby_number(*engine, "Perception::SIGHT")).epsilon(kTolerance));
  CHECK(kPerceptionFov == doctest::Approx(ruby_number(*engine, "Perception::FOV")).epsilon(kTolerance));
  CHECK(kPerceptionEyes == doctest::Approx(ruby_number(*engine, "Perception::EYES")).epsilon(kTolerance));
  CHECK(kPerceptionHearing == doctest::Approx(ruby_number(*engine, "Perception::HEARING")).epsilon(kTolerance));
  const RubyResult talk = engine->scripts().eval("Talkable::PROMPT", "npc_settings_test.rb");
  REQUIRE(talk.ok);
  CHECK(std::string(RSTRING_PTR(talk.value), RSTRING_LEN(talk.value)) == kTalkPromptKey);
}
