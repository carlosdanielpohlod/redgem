#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <filesystem>
#include <fstream>

#include "app/scene_files.hpp"
#include "app/touch_circles.hpp"
#include "app/engine.hpp"
#include "core/config.hpp"
#include "core/exit_codes.hpp"
#include "app/window_presenter.hpp"
#include "testing/recording_audio.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kOneSecondOfFrames = 60;
constexpr int kSettleFrames = 120;
constexpr int kJumpRiseFrames = 10;
constexpr int kBrokenRunFrames = 10;
constexpr int kStartupFrames = 3;
constexpr float kPositionTolerance = 0.05f;
constexpr float kMinWalkDistance = 3.0f;
constexpr float kMinJumpHeight = 0.5f;
constexpr float kCrateRestHeight = 0.5f;  // half of the 1m crate above ground level 0
constexpr float kLiftSpeed = 1.0f;        // tests/fixtures/games/sandbox/scripts/lift.rb
constexpr float kTeleportX = -2.0f;
constexpr float kTeleportMinY = 5.0f;
constexpr float kFallenBelowY = -5.0f;
constexpr int kExpectedBehaviours = 9;    // probe, coin, lift, family, kid, gc_stress, sensor, spawner, broken
constexpr int kExpectedScriptErrors = 2;  // the missing script and the broken one
constexpr float kPushSpeed = 5.0f;
constexpr float kQuarterTurnDegrees = 90.0f;
constexpr float kTeleportHeadroom = 1.0f;

// The sandbox game shown by nobody, a fixed step each frame, playing through `audio` when one is given.
std::unique_ptr<Engine> sandbox(AudioSystem* audio = nullptr) {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  options.audio = audio;
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

Vec3 position_of(Engine& engine, const std::string& name) {
  const ObjectId id = engine.scene().find(name);
  REQUIRE(id != kNoObject);
  return engine.scene().world_transform(id).position;
}

}  // namespace

TEST_CASE("sandbox attaches behaviours and reports the missing script") {
  test::LogCapture capture;
  auto engine = sandbox();
  CHECK(behaviour_count(*engine) == kExpectedBehaviours);
  CHECK(engine->scripts().error_count() == 1);
  CHECK(capture.contains("does_not_exist.rb"));
}

TEST_CASE("start runs before the first update and callbacks keep running") {
  auto engine = sandbox();
  frames(*engine, kStartupFrames);
  const std::string expected = std::to_string(kStartupFrames);
  CHECK(ruby(*engine, "$probe[:started]") == "true");
  CHECK(ruby(*engine, "$probe[:updates]") == expected);
  CHECK(ruby(*engine, "$probe[:fixed_updates]") == expected);
  CHECK(ruby(*engine, "Time.frame") == expected);
}

TEST_CASE("a failing behaviour is disabled and the game keeps going") {
  test::LogCapture capture;
  auto engine = sandbox();
  frames(*engine, kBrokenRunFrames);
  CHECK(ruby(*engine, "$broken_calls") == "1");
  CHECK(capture.contains("boom from Broken"));
  CHECK(engine->scripts().error_count() == kExpectedScriptErrors);
  CHECK(ruby(*engine, "$probe[:updates]") == std::to_string(kBrokenRunFrames));
}

TEST_CASE("dynamic bodies fall and land") {
  auto engine = sandbox();
  frames(*engine, kSettleFrames);
  CHECK(position_of(*engine, "Crate").y == doctest::Approx(kCrateRestHeight).epsilon(kPositionTolerance));
}

TEST_CASE("the character walks with input, hits the coin trigger and the coin is destroyed") {
  auto engine = sandbox();
  frames(*engine, kOneSecondOfFrames);
  CHECK(ruby(*engine, "$probe[:collisions].include?('Ground')") == "true");
  const float start_x = position_of(*engine, "Hero").x;

  engine->scripted_input().set_key(Key::D, true);
  frames(*engine, kOneSecondOfFrames);
  engine->scripted_input().release_all();

  CHECK(position_of(*engine, "Hero").x > start_x + kMinWalkDistance);
  CHECK(ruby(*engine, "$probe[:triggers].first") == "enter:Coin");
  CHECK(ruby(*engine, "$coin_touched_by") == "Hero");
  CHECK(ruby(*engine, "$coin_destroyed") == "true");
  CHECK(engine->scene().find("Coin") == kNoObject);
  CHECK(ruby(*engine, "Scene.find_all(:coin).size") == "0");
}

TEST_CASE("jumping leaves the ground") {
  auto engine = sandbox();
  frames(*engine, kOneSecondOfFrames);
  const float ground_y = position_of(*engine, "Hero").y;
  engine->scripted_input().set_key(Key::Space, true);
  frames(*engine, 1);
  engine->scripted_input().release_all();
  frames(*engine, kJumpRiseFrames);
  CHECK(position_of(*engine, "Hero").y > ground_y + kMinJumpHeight);
  CHECK(ruby(*engine, "Scene.find('Hero').character.grounded?") == "false");
  CHECK(ruby(*engine, "$probe[:collision_exits].include?('Ground')") == "true");
}

TEST_CASE("turning while walking keeps the jump going") {
  auto engine = sandbox();
  frames(*engine, kOneSecondOfFrames);
  const float ground_y = position_of(*engine, "Hero").y;
  engine->scripted_input().set_key(Key::D, true);  // probe.rb faces the movement every frame
  engine->scripted_input().set_key(Key::Space, true);
  frames(*engine, 1);
  engine->scripted_input().set_key(Key::Space, false);
  frames(*engine, kJumpRiseFrames);
  engine->scripted_input().release_all();
  CHECK(position_of(*engine, "Hero").y > ground_y + kMinJumpHeight);
}

TEST_CASE("rotating a moving rigidbody keeps its velocity") {
  auto engine = sandbox();
  frames(*engine, kSettleFrames);
  ruby(*engine, "$crate = Scene.find('Crate'); $crate.rigidbody.velocity = Vector3.new(" + std::to_string(kPushSpeed) +
                    ", 0, 0)");
  ruby(*engine, "$crate.transform.rotation = Vector3.new(0, " + std::to_string(kQuarterTurnDegrees) + ", 0)");
  CHECK(std::stof(ruby(*engine, "$crate.rigidbody.velocity.x")) == doctest::Approx(kPushSpeed));
}

TEST_CASE("kinematic platforms move from scripts") {
  auto engine = sandbox();
  const float start_y = position_of(*engine, "Lift").y;
  frames(*engine, kOneSecondOfFrames);
  CHECK(position_of(*engine, "Lift").y == doctest::Approx(start_y + kLiftSpeed).epsilon(kPositionTolerance));
}

TEST_CASE("scripts teleport physics objects by assigning the position") {
  auto engine = sandbox();
  frames(*engine, 1);
  ruby(*engine, "Scene.find('Hero').transform.position = Vector3.new(" + std::to_string(kTeleportX) + ", " +
                    std::to_string(kTeleportMinY + kTeleportHeadroom) + ", 0)");
  frames(*engine, 1);
  const Vec3 hero = position_of(*engine, "Hero");
  CHECK(hero.x == doctest::Approx(kTeleportX).epsilon(kPositionTolerance));
  CHECK(hero.y > kTeleportMinY);
}

TEST_CASE("props, tags and lookups are visible from Ruby") {
  auto engine = sandbox();
  CHECK(ruby(*engine, "Scene.find('Hero').props[:speed]") == "4.0");
  CHECK(ruby(*engine, "Scene.find('Crate').tag") == "crate");
  CHECK(ruby(*engine, "Scene.find('Crate').rigidbody.nil?") == "false");
  CHECK(ruby(*engine, "Scene.find('Ground').rigidbody.nil?") == "true");
  CHECK(ruby(*engine, "Camera.main.name") == "Camera");
  CHECK(ruby(*engine, "Scene.find('Hero').get_component(Probe).class") == "Probe");
  CHECK(ruby(*engine, "Scene.find('Hero') == Scene.find('Hero')") == "true");
}

TEST_CASE("children destroyed with their parent still see it in on_destroy") {
  auto engine = sandbox();
  frames(*engine, 1);
  ruby(*engine, "Scene.find('Family').destroy");
  frames(*engine, 1);
  CHECK(ruby(*engine, "$kid_saw_parent") == "Family");
  CHECK(ruby(*engine, "$kid_saw_family") == "Family");  // the parent's components are still attached
  CHECK(engine->scene().find("Kid") == kNoObject);
  CHECK(engine->scripts().error_count() == kExpectedScriptErrors);
}

TEST_CASE("natives reject invalid objects with Ruby errors instead of crashing") {
  test::LogCapture capture;
  auto engine = sandbox();
  const RubyResult result = engine->scripts().eval(
      "GameObject.allocate.tap { |o| o.instance_variable_set(:@id, 9999); o.instance_variable_set(:@serial, 1) "
      "}.visible",
      "bad.rb");
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("9999") != std::string::npos);
  const RubyResult destroyed =
      engine->scripts().eval("kid = Scene.find('Kid'); kid.destroy; $kid_ref = kid", "destroy.rb");
  REQUIRE(destroyed.ok);
  frames(*engine, 1);
  CHECK_FALSE(engine->scripts().eval("$kid_ref.transform.position", "dead.rb").ok);
  CHECK(ruby(*engine, "$kid_ref.alive?") == "false");
}

TEST_CASE("static geometry does not fire triggers") {
  auto engine = sandbox();
  frames(*engine, kSettleFrames);
  CHECK(ruby(*engine, "$sensor_hits.inspect") == "[]");
}

TEST_CASE("moving a static body from a script moves its collider") {
  auto engine = sandbox();
  frames(*engine, kOneSecondOfFrames);
  ruby(*engine, "g = Scene.find('Ground'); g.transform.position = g.transform.position - Vector3.up * 100");
  frames(*engine, kOneSecondOfFrames);
  CHECK(position_of(*engine, "Hero").y < kFallenBelowY);
}

TEST_CASE("objects survive garbage collection every frame") {
  auto engine = sandbox();  // the GcStress behaviour runs GC.start in every callback
  frames(*engine, kSettleFrames);
  CHECK(ruby(*engine, "Scene.find('Hero').get_component(Probe).class") == "Probe");
  CHECK(ruby(*engine, "Scene.objects.map(&:name).include?('Crate')") == "true");
}

TEST_CASE("a game's Ruby has nothing of what tests are written with") {
  // This program links the engine alone, as the one games ship as does: what tests use comes from
  // another library (src/testing), which a game never runs with.
  auto engine = sandbox();
  for (const char* name : {"TestInput", "SystemTest", "RedGemTest"}) {
    CAPTURE(name);
    CHECK(ruby(*engine, std::string("Object.const_defined?(:") + name + ")") == "false");
  }
  CHECK_FALSE(engine->scripts().eval("TestInput.set(:a, true)", "input.rb").ok);
  CHECK(ruby(*engine, "$__redgem_system_test.nil?") == "true");
}

TEST_CASE("an engine told to step fixedly plays the same game whatever time passes") {
  constexpr float kStall = 0.5f;  // a frame that took half a second
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  ScriptedInputSource input;
  options.input = &input;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  for (int i = 0; i < kOneSecondOfFrames; ++i) engine.frame(kStall);
  CHECK(engine.script_context().time.elapsed == doctest::Approx(kOneSecondOfFrames * config::kFixedDeltaSeconds));
}

TEST_CASE("only fixed_step selects fixed stepping: without it the game follows the time it is given") {
  constexpr float kHalfStep = config::kFixedDeltaSeconds / 2.0f;
  constexpr int kFrames = 20;
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");  // (no input, no presenter, no audio: none of them decides)
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  for (int i = 0; i < kFrames; ++i) engine.frame(kHalfStep);
  CHECK(engine.script_context().time.elapsed == doctest::Approx(kFrames * kHalfStep).epsilon(0.1));
}

TEST_CASE("the input a program hands the engine is what the game reads") {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  ScriptedInputSource input;
  options.input = &input;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  input.set_key(Key::A, true);
  engine.frame(config::kFixedDeltaSeconds);
  CHECK(ruby(engine, "Input.down?(:a)") == "true");
}

TEST_CASE("prefabs stay inert until spawned, then get physics and scripts") {
  constexpr int kFallFrames = 60;
  constexpr float kSpawnHeight = 6.0f;
  auto engine = sandbox();
  frames(*engine, 1);
  CHECK(ruby(*engine, "Scene.find_all(:bullet).size") == "0");  // the template is not a live bullet
  CHECK(ruby(*engine, "$bullet_starts.inspect") == "nil");      // and runs no script
  const ObjectId prefab = engine->scene().find("Bullet");
  CHECK_FALSE(engine->physics().has_object(prefab));

  ruby(*engine, "$shot = Scene.spawn('Bullet', position: Vector3.new(2, " + std::to_string(kSpawnHeight) + ", 0))");
  frames(*engine, kFallFrames);
  CHECK(ruby(*engine, "$bullet_starts") == "1");
  CHECK(ruby(*engine, "Scene.find_all(:bullet).size") == "1");
  CHECK(std::stof(ruby(*engine, "$shot.transform.position.y")) < kSpawnHeight - 1.0f);  // it fell: physics works
  CHECK(engine->scene().world_transform(prefab).position.y > 0.0f);                     // the template did not move
}

TEST_CASE("objects spawned from inside update start on the next frame") {
  constexpr int kSpawnFrames = 20;
  auto engine = sandbox();
  frames(*engine, 1);
  ruby(*engine, "$spawner_on = true");
  frames(*engine, kSpawnFrames);
  ruby(*engine, "$spawner_on = false");
  frames(*engine, 1);
  CHECK(ruby(*engine, "Scene.find_all(:bullet).size") == std::to_string(kSpawnFrames));
  CHECK(ruby(*engine, "$bullet_starts") == std::to_string(kSpawnFrames));
  CHECK(engine->scripts().error_count() == kExpectedScriptErrors);
}

TEST_CASE("a kept GameObject stands for its own object only, even once its slot holds a new one") {
  auto engine = sandbox();
  frames(*engine, 1);
  ruby(*engine, "$old = Scene.spawn('Bullet', physics: false); $old_transform = $old.transform");
  ruby(*engine, "$old.destroy");
  frames(*engine, 1);
  ruby(*engine, "$new = Scene.spawn('Bullet', physics: false)");
  REQUIRE(ruby(*engine, "$new.id == $old.id") == "true");  // the slot was used again
  CHECK(ruby(*engine, "$old.alive?") == "false");
  CHECK(ruby(*engine, "$old == $new") == "false");
  CHECK(ruby(*engine, "$old.name") == "Bullet");  // still readable
  CHECK_FALSE(engine->scripts().eval("$old.transform.position", "stale.rb").ok);
  CHECK_FALSE(engine->scripts().eval("$old_transform.position = Vector3.zero", "stale.rb").ok);
  CHECK(ruby(*engine, "$old.destroy; $new.alive?") == "true");  // destroying it again leaves the new one be
  const RubyResult moved = engine->scripts().eval("$old_transform.position", "stale.rb");
  CHECK(moved.error.find("was destroyed") != std::string::npos);
}

TEST_CASE("a GameObject kept in a global across a scene switch stands for nothing in the new scene") {
  auto engine = sandbox();
  frames(*engine, 1);
  ruby(*engine, "$hero = Scene.find('Hero'); Game.load_scene('level2')");
  frames(*engine, 1);
  CHECK(ruby(*engine, "$hero.alive?") == "false");
  CHECK(ruby(*engine, "$hero.name") == "Hero");
  CHECK_FALSE(engine->scripts().eval("$hero.transform.position", "stale.rb").ok);
}

TEST_CASE("spawning and destroying for a long time does not grow the scene or the scripts") {
  constexpr int kRounds = 300;
  auto engine = sandbox();
  frames(*engine, 1);
  ruby(*engine, "def churn = 5.times { Scene.spawn('Bullet', physics: false).destroy }");
  ruby(*engine, "churn");
  frames(*engine, 1);
  const std::size_t slots = engine->scene().size();
  for (int i = 0; i < kRounds; ++i) {
    ruby(*engine, "churn");
    frames(*engine, 1);
  }
  CHECK(engine->scene().size() == slots);
  CHECK(engine->scripts().error_count() == kExpectedScriptErrors);
}

TEST_CASE("a game whose scene cannot be read says so before any window is opened") {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem_engine_broken_scene";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game);
  std::ofstream(game / config::kSceneFileName) << "this is no scene";
  CHECK(std::holds_alternative<std::string>(read_game_scene(game)));
  CHECK(std::holds_alternative<SceneDesc>(read_game_scene(fixture("games/sandbox"))));
  test::LogCapture log;
  GameWindowOptions options;
  options.game_dir = game;
  CHECK(play_in_window(options) == kExitFailure);
  CHECK(log.count(log::Level::Error) == 1);
  CHECK(log.contains(config::kSceneFileName));
  // A window says which graphics card it opened on: none did.
  CHECK_FALSE(log.contains("graphics card"));
  CHECK(log.count(log::Level::Info) == 0);
  ScriptedInputSource input;
  CHECK(play_in_window(options, input) == kExitFailure);  // the same for a screen with devices of its own
  CHECK_FALSE(log.contains("graphics card"));
  std::filesystem::remove_all(game);
}

TEST_CASE("a game whose id cannot be used still plays, and is told") {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem_engine_bad_id";
  std::filesystem::remove_all(game);
  std::filesystem::copy(fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
  std::ofstream(game / kGameSettingsFile) << R"({"id": "Not An Id"})";
  test::LogCapture log;
  EngineOptions options;
  options.game_dir = game;
  options.fixed_step = true;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  CHECK(log.contains("game.json: id must look like"));
  CHECK(log.contains("the game's save goes by where its folder is"));
  std::filesystem::remove_all(game);
}

TEST_CASE("Game.title is what the game is called: its folder's name unless game.json says another") {
  auto engine = sandbox();
  CHECK(ruby(*engine, "Game.title") == "sandbox");
}

TEST_CASE("Game.load_scene switches scenes at the end of the frame and keeps Ruby globals") {
  auto engine = sandbox();
  frames(*engine, 1);
  CHECK(ruby(*engine, "Game.scene_name") == "main");
  ruby(*engine, "$lives = 3; Game.load_scene('level2')");
  CHECK(engine->scene().find("Hero") != kNoObject);  // still the old scene until the frame ends
  frames(*engine, 1);
  CHECK(engine->scene_name() == "level2");
  CHECK(engine->scene().find("Hero") == kNoObject);
  CHECK(engine->scene().find("Level2Marker") != kNoObject);
  frames(*engine, 1);
  CHECK(ruby(*engine, "$level2_started") == "true");
  CHECK(ruby(*engine, "$lives") == "3");
  CHECK(ruby(*engine, "Game.scene_name") == "level2");
  CHECK_FALSE(engine->scripts().eval("Game.load_scene('nowhere')", "bad_scene.rb").ok);
  ruby(*engine, "Game.load_scene('main')");
  frames(*engine, 1);
  CHECK(engine->scene().find("Hero") != kNoObject);
}

TEST_CASE("sounds are looked up in the game's sounds folder") {
  constexpr float kHalfVolume = 0.5f;
  testing::RecordingAudio recording;  // made first: the engine plays through it until its end
  auto engine = sandbox(&recording);
  CHECK(ruby(*engine, "Audio.play(:beep)") == "true");
  CHECK(ruby(*engine, "Audio.play(:nope)") == "false");
  CHECK(ruby(*engine, "Audio.music('beep', loop: false)") == "true");
  CHECK(recording.played() == std::vector<std::string>{"beep"});
  CHECK(recording.music() == "beep");
  // Music can be bent while it plays (an engine's note following the car's speed).
  ruby(*engine, "Audio.music_pitch = 1.5; Audio.music_volume = 0.25");
  CHECK(recording.music_pitch() == doctest::Approx(1.5f));
  CHECK(recording.music_volume() == doctest::Approx(0.25f));
  CHECK_FALSE(engine->scripts().eval("Audio.music_pitch = 0", "bad_pitch.rb").ok);
  ruby(*engine, "Audio.stop_music; Audio.volume = " + std::to_string(kHalfVolume));
  CHECK_FALSE(recording.music().has_value());
  CHECK(recording.master_volume() == doctest::Approx(kHalfVolume));
}

TEST_CASE("an engine without an audio of its options is silent") {
  auto engine = sandbox();
  const int errors_before = engine->scripts().error_count();
  CHECK(ruby(*engine, "Audio.play(:beep)") == "false");
  CHECK(ruby(*engine, "Audio.music('beep', loop: false)") == "false");
  ruby(*engine, "Audio.music_pitch = 1.5; Audio.stop_music; Audio.volume = 0.5");  // asked of nobody: no harm
  CHECK(engine->scripts().error_count() == errors_before);
}

TEST_CASE("the frames-per-second counter is for whoever asks for it") { CHECK_FALSE(GameWindowOptions{}.show_fps); }

TEST_CASE("the scene's animations play: bones move over time and loop") {
  constexpr int kHalfSecondFrames = 30;
  constexpr float kMinTurn = 0.2f;  // of the quaternion's x: the upper bone bends around X
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem_animated_game";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game);
  std::filesystem::copy_file(fixture("skinned.glb"), game / "scene.glb");
  EngineOptions options;
  options.game_dir = game;
  options.fixed_step = true;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  const ObjectId upper = engine.scene().find("Upper");
  REQUIRE(upper != kNoObject);
  const Quat rest = engine.scene().at(upper).local.rotation;
  for (int i = 0; i < kHalfSecondFrames; ++i) engine.frame(config::kFixedDeltaSeconds);
  const Quat bent = engine.scene().at(upper).local.rotation;
  CHECK(std::fabs(bent.x - rest.x) > kMinTurn);
  // The clip lasts one second: half a second later it starts over.
  for (int i = 0; i < kHalfSecondFrames + 1; ++i) engine.frame(config::kFixedDeltaSeconds);
  CHECK(std::fabs(engine.scene().at(upper).local.rotation.x - rest.x) < kMinTurn);
  std::filesystem::remove_all(game);
}

TEST_CASE("a touch screen's controls reach the renderer as plain circles, the stick's ring under the rest") {
  TouchLayout layout;
  layout.stick_base = {100.0f, 500.0f, 90.0f, true};  // (a finger on the ring does not light it up)
  layout.stick_knob = {120.0f, 480.0f, 40.0f, true};
  layout.a = {1100.0f, 520.0f, 50.0f, false};
  layout.b = {1200.0f, 440.0f, 50.0f, true};
  layout.start = {640.0f, 60.0f, 30.0f, false};
  const std::vector<ControlCircle> circles = control_circles(layout);
  REQUIRE(circles.size() == 5);
  CHECK(circles[0].look == ControlCircle::Look::Base);
  CHECK(circles[0].radius == doctest::Approx(layout.stick_base.radius));
  CHECK(circles[1].look == ControlCircle::Look::Pressed);
  CHECK(circles[1].x == doctest::Approx(layout.stick_knob.x));
  CHECK(circles[2].look == ControlCircle::Look::Resting);
  CHECK(circles[3].look == ControlCircle::Look::Pressed);
  CHECK(circles[4].y == doctest::Approx(layout.start.y));
}
