#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <filesystem>
#include <fstream>
#include <variant>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "script/script_naming.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;
using redgem::test::kTolerance;

namespace {

constexpr int kOneSecondOfFrames = 60;
constexpr double kTunedSpeed = 5.0;    // the "Tuned" object's own property
constexpr float kWalkerSpeed = 2.0f;   // tests/fixtures/games/directed/scripts/main_scene.rb
constexpr float kDefaultSpeed = 1.0f;  // .../mover.rb
constexpr float kTravelTolerance = 0.02f;
constexpr int kMovers = 3;

// Three bare objects: everything they do comes from the scene script.
SceneDesc bare_scene() {
  SceneDesc desc;
  for (const char* name : {"Walker", "Tuned", "Plain"}) {
    NodeDesc node;
    node.name = name;
    desc.nodes.push_back(node);
  }
  desc.nodes[1].props = {{"speed", kTunedSpeed}};
  return desc;
}

std::unique_ptr<Engine> directed(const SceneDesc& scene) {
  EngineOptions options;
  options.game_dir = fixture("games/directed");
  options.fixed_step = true;
  options.scene = &scene;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

void frames(Engine& engine, int count) {
  for (int i = 0; i < count; ++i) engine.frame(config::kFixedDeltaSeconds);
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

// How many behaviours run in the game, counted where a game would count them.
int behaviour_count(Engine& engine) {
  return std::stoi(ruby(engine, "Scene.objects.inject(0) { |total, object| total + object.behaviours.size }"));
}

float x_of(Engine& engine, const std::string& name) {
  const ObjectId id = engine.scene().find(name);
  REQUIRE(id != kNoObject);
  return engine.scene().world_transform(id).position.x;
}

}  // namespace

TEST_CASE("class names map back to script files") {
  CHECK(script_for_class_name("Player") == "player.rb");
  CHECK(script_for_class_name("CameraFollow") == "camera_follow.rb");
  CHECK(class_name_for_script(script_for_class_name("SlimeKing")) == "SlimeKing");
  CHECK(scene_script_for("main") == "main_scene.rb");
  CHECK(scene_script_for("level2") == "level2_scene.rb");
}

TEST_CASE("the scene script adds behaviours before anything starts") {
  const SceneDesc scene = bare_scene();
  auto engine = directed(scene);
  CHECK(engine->scripts().error_count() == 0);
  CHECK(behaviour_count(*engine) == kMovers);
  CHECK(ruby(*engine, "$scene_setups") == "1");
  CHECK(ruby(*engine, "$movers_started_before_setup.inspect") == "nil");
  CHECK(ruby(*engine, "$mover_starts.inspect") == "nil");  // start waits for the first frame
  frames(*engine, 1);
  CHECK(ruby(*engine, "$mover_starts") == std::to_string(kMovers));
}

TEST_CASE("settings given to add_component come before the object's properties and the defaults") {
  const SceneDesc scene = bare_scene();
  auto engine = directed(scene);
  frames(*engine, kOneSecondOfFrames);
  CHECK(x_of(*engine, "Walker") == doctest::Approx(kWalkerSpeed).epsilon(kTravelTolerance));
  CHECK(x_of(*engine, "Tuned") == doctest::Approx(kTunedSpeed).epsilon(kTravelTolerance));
  CHECK(x_of(*engine, "Plain") == doctest::Approx(kDefaultSpeed).epsilon(kTravelTolerance));
  CHECK(ruby(*engine, "Scene.find('Walker').get_component(Mover).speed") == "2.0");
}

TEST_CASE("add_component works while the game runs and returns the behaviour") {
  const SceneDesc scene = bare_scene();
  auto engine = directed(scene);
  frames(*engine, 1);
  CHECK(ruby(*engine, "Scene.find('Plain').add_component(Mover, speed: 3.0).class") == "Mover");
  CHECK(behaviour_count(*engine) == kMovers + 1);
  CHECK(ruby(*engine, "Scene.find('Plain').behaviours.size") == "2");
  frames(*engine, 1);
  CHECK(ruby(*engine, "$mover_starts") == std::to_string(kMovers + 1));
}

TEST_CASE("add_component refuses what is not a Behaviour class") {
  test::LogCapture capture;
  const SceneDesc scene = bare_scene();
  auto engine = directed(scene);
  CHECK(ruby(*engine, "Scene.find('Plain').add_component(Stride).inspect") == "nil");
  CHECK(ruby(*engine, "Scene.find('Plain').add_component('mover.rb').inspect") == "nil");
  CHECK(engine->scripts().error_count() == 2);
  CHECK(capture.contains("must inherit from Behaviour"));
  CHECK(behaviour_count(*engine) == kMovers);
}

TEST_CASE("classes of the game's scripts load on first use; unknown names still fail") {
  const SceneDesc scene = bare_scene();
  auto engine = directed(scene);
  CHECK(ruby(*engine, "Stride.length(2.0, 3.0)") == "6.0");
  const RubyResult missing = engine->scripts().eval("NoSuchScriptClass.new", "missing.rb");
  CHECK_FALSE(missing.ok);
  CHECK(missing.error.find("uninitialized constant NoSuchScriptClass") != std::string::npos);
  // A script that needs itself to load reports an error instead of recursing.
  CHECK_FALSE(engine->scripts().eval("Selfish.new", "selfish_use.rb").ok);
}

TEST_CASE("each scene runs its own script when it loads") {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  frames(engine, 1);
  CHECK(ruby(engine, "$level2_setup_in.inspect") == "nil");  // the sandbox's main scene has no script
  ruby(engine, "Game.load_scene('level2')");
  frames(engine, 1);
  CHECK(ruby(engine, "$level2_setup_in") == "level2");
}

TEST_CASE("a scene script of the wrong kind is reported") {
  test::LogCapture capture;
  const SceneDesc scene = bare_scene();
  EngineOptions options;
  options.game_dir = fixture("games/misdirected");
  options.fixed_step = true;
  options.scene = &scene;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  CHECK(std::get<std::unique_ptr<Engine>>(created)->scripts().error_count() == 1);
  CHECK(capture.contains("must inherit from SceneScript"));
}

TEST_CASE("an object's script is a file of the game's scripts folder, or it is not loaded") {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem-script-outside-test";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "scripts");
  std::ofstream(game / "outside.rb") << "$outside_ran = true\nclass Outside < Behaviour\nend\n";
  std::ofstream(game / "scripts" / "inside.rb") << "$inside_ran = true\nclass Inside < Behaviour\nend\n";
  SceneDesc scene = bare_scene();
  scene.nodes[0].props = {{"script", std::string("../outside.rb")}};
  scene.nodes[1].props = {{"script", std::string((game / "outside.rb").string())}};
  scene.nodes[2].props = {{"script", std::string("inside.rb")}};
  test::LogCapture capture;
  EngineOptions options;
  options.game_dir = game;
  options.fixed_step = true;
  options.scene = &scene;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  CHECK(ruby(engine, "$outside_ran.inspect") == "nil");
  CHECK(ruby(engine, "$inside_ran.inspect") == "true");
  CHECK(engine.scripts().error_count() == 2);
  CHECK(capture.contains("outside the scripts folder"));
  // Nor does a scene outside the game's folders load.
  CHECK_FALSE(engine.scripts().eval("Game.load_scene('../../elsewhere')", "test.rb").ok);
  std::filesystem::remove_all(game);
}

// A constant of the class's name that is not a class is the script's mistake, reported like a
// missing class: asking mruby for it as a class would raise where nothing catches it.
TEST_CASE("a script's class name given to something that is not a class is reported, not fatal") {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem-not-a-class-test";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "scripts");
  std::ofstream(game / "scripts" / "player.rb") << "Player = 5\n";
  SceneDesc scene = bare_scene();
  scene.nodes[0].props = {{"script", std::string("player.rb")}};
  test::LogCapture capture;
  EngineOptions options;
  options.game_dir = game;
  options.fixed_step = true;
  options.scene = &scene;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  CHECK(engine.scripts().error_count() == 1);
  CHECK(capture.contains("does not define class Player"));
  frames(engine, 1);
  std::filesystem::remove_all(game);
}

TEST_CASE("a scene script's class replaced by something else is reported, not fatal") {
  test::LogCapture capture;
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  for (const char* scene : {"level2", "main"}) {  // level2's script is loaded once and for all
    ruby(engine, std::string("Game.load_scene('") + scene + "')");
    frames(engine, 1);
  }
  ruby(engine, "Level2Scene = 5; Game.load_scene('level2')");
  frames(engine, 1);
  CHECK(engine.scene_name() == "level2");
  CHECK(capture.contains("does not define class Level2Scene"));
}
