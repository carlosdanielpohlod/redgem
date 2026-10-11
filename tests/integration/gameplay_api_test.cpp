#include <doctest/doctest.h>

#include <filesystem>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kFramesPerSecond = 60;
constexpr int kHalfSecond = kFramesPerSecond / 2;
constexpr float kClipSeconds = 2.0f;
constexpr float kClipDistance = 10.0f;
constexpr float kAjar = 6.0f;  // where the "Ajar" clip holds the door
constexpr float kStickHalfway = 0.5f;
constexpr int kFewestTweenCalls = 30;  // half a second of frames, give or take one, plus the start
constexpr int kMostTweenCalls = 32;
constexpr int kLongWait = kFramesPerSecond * 6;  // past the five-second timer

// A camera, an object to hang behaviours on, and a door whose "Open" clip slides it along x.
SceneDesc small_scene() {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  NodeDesc host;
  host.name = "Host";
  NodeDesc door;
  door.name = "Door";
  desc.nodes = {camera, host, door};
  AnimationChannel slide;
  slide.node = 2;
  slide.times = {0.0f, kClipSeconds};
  slide.values = {0, 0, 0, kClipDistance, 0, 0};
  AnimationDesc open;
  open.name = "Open";
  open.duration = kClipSeconds;
  open.channels = {slide};
  desc.animations = {open};
  return desc;
}

// A save file of the test's own, removed when the test ends.
struct TempSave {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-gameplay-api-test";
  std::filesystem::path file = dir / "save.json";
  TempSave() { std::filesystem::remove_all(dir); }
  ~TempSave() { std::filesystem::remove_all(dir); }
};

std::unique_ptr<Engine> engine_for(const SceneDesc& scene, const std::filesystem::path& save_file) {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.headless = true;
  options.scene = &scene;
  options.save_file = save_file;
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

bool fails(Engine& engine, const std::string& code) { return !engine.scripts().eval(code, "bad.rb").ok; }

float door_x(Engine& engine) { return engine.scene().at(engine.scene().find("Door")).local.position.x; }

}  // namespace

TEST_CASE("after runs a block once, later; every keeps running it; both can be cancelled") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  ruby(*engine, R"(
    $once = 0; $ticks = 0; $never = 0
    class Clockwork < Behaviour
      def start
        after(1.0) { $once += 1 }
        $ticker = every(0.2) { $ticks += 1 }
        after(0.5) { $never += 1 }.cancel
      end
    end
    Scene.find('Host').add_component(Clockwork)
  )");
  frames(*engine, kHalfSecond);
  CHECK(ruby(*engine, "[$once, $ticks, $never]") == "[0, 2, 0]");
  frames(*engine, kFramesPerSecond);
  CHECK(ruby(*engine, "[$once, $ticks, $never]") == "[1, 7, 0]");
  ruby(*engine, "$ticker.cancel");
  frames(*engine, kFramesPerSecond);
  CHECK(ruby(*engine, "[$once, $ticks, $ticker.cancelled?]") == "[1, 7, true]");
}

TEST_CASE("a tween calls its block every frame, from 0 to exactly 1") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  ruby(*engine, R"(
    $smooth = []; $linear = []
    class Grower < Behaviour
      def start
        tween(0.5) { |t| $smooth << t }
        tween(0.5, ease: :linear) { |t| $linear << t }
      end
    end
    $grower = Scene.find('Host').add_component(Grower)
  )");
  frames(*engine, kFramesPerSecond);
  CHECK(ruby(*engine, "$smooth.size == $linear.size") == "true");
  CHECK(std::stoi(ruby(*engine, "$smooth.size")) >= kFewestTweenCalls);
  CHECK(std::stoi(ruby(*engine, "$smooth.size")) <= kMostTweenCalls);
  CHECK(ruby(*engine, "[$smooth.first, $linear.first, $smooth.last, $linear.last]") == "[0.0, 0.0, 1.0, 1.0]");
  CHECK(ruby(*engine, "$smooth == $smooth.sort && $linear == $linear.sort") == "true");  // only ever forwards
  // A smooth tween starts gently: behind a steady one early on, and level with it halfway.
  CHECK(ruby(*engine, "$smooth[4] < $linear[4]") == "true");
  CHECK(ruby(*engine, "half = $linear.index { |t| t >= 0.5 }; ($smooth[half] - $linear[half]).abs < 0.02") == "true");
  CHECK(fails(*engine, "$grower.tween(1.0, ease: :bouncy) { }"));
}

TEST_CASE("timers wait while the game is paused, and end with their object") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  ruby(*engine, R"(
    $ticks = 0
    class Ticker < Behaviour
      def start = every(0.1) { $ticks += 1 }
    end
    Scene.find('Host').add_component(Ticker)
  )");
  frames(*engine, kHalfSecond);
  const std::string before = ruby(*engine, "$ticks");
  CHECK(before != "0");
  ruby(*engine, "Time.scale = 0");
  frames(*engine, kFramesPerSecond);
  CHECK(ruby(*engine, "$ticks") == before);
  ruby(*engine, "Time.scale = 1; Scene.find('Host').destroy");
  frames(*engine, kFramesPerSecond);
  CHECK(ruby(*engine, "$ticks") == before);
}

TEST_CASE("timers started by a timer wait for the next frame, and cancel_timers calls them all off") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  ruby(*engine, R"(
    $chain = 0; $late = 0
    class Chain < Behaviour
      def start
        after(0.0) { $chain += 1; after(0.0) { $chain += 1 } }
        after(5.0) { $late += 1 }
      end
    end
    $chain_behaviour = Scene.find('Host').add_component(Chain)
  )");
  frames(*engine, 1);  // the behaviour starts and is updated for the first time
  CHECK(ruby(*engine, "$chain") == "1");
  frames(*engine, 1);
  CHECK(ruby(*engine, "$chain") == "2");
  ruby(*engine, "$chain_behaviour.cancel_timers");
  frames(*engine, kLongWait);
  CHECK(ruby(*engine, "$late") == "0");
}

TEST_CASE("a timer can call off the others, and itself; a tiny interval does not run away") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  ruby(*engine, R"(
    $ticks = 0; $flood = 0
    class Quitter < Behaviour
      def start
        every(0.1) { $ticks += 1 }
        after(0.25) { cancel_timers }
      end
    end
    class Flood < Behaviour
      def start = every(0.000001) { $flood += 1 }
    end
    Scene.find('Host').add_component(Quitter)
    Scene.find('Door').add_component(Flood)
  )");
  frames(*engine, 1);
  CHECK(ruby(*engine, "$flood") == "8");  // a few times a frame at most
  frames(*engine, kFramesPerSecond);
  CHECK(ruby(*engine, "$ticks") == "2");
}

TEST_CASE("what scripts save is there in the next run") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  {
    auto engine = engine_for(scene, temp.file);
    CHECK(ruby(*engine, "Save.get(:best).inspect") == "nil");
    CHECK(ruby(*engine, "Save.get(:best, 0)") == "0");
    ruby(*engine, "Save.set(:best, 1200); Save[:name] = 'Ana'; Save[:muted] = true; Save[:volume] = 0.5");
    CHECK(fails(*engine, "Save[:broken] = Float::INFINITY"));
    CHECK(ruby(*engine, "Save[:huge] = 1.0e300; Save[:huge]") == "1.0e+300");
    ruby(*engine, "Save.delete(:huge)");
    CHECK(fails(*engine, "Save[:position] = Vector3.zero"));  // only numbers, texts and true/false
  }
  auto engine = engine_for(scene, temp.file);
  CHECK(ruby(*engine, "[Save[:best], Save[:name], Save[:muted], Save[:volume]].inspect") ==
        "[1200, \"Ana\", true, 0.5]");
  CHECK(ruby(*engine, "Save.keys.inspect") == "[:best, :muted, :name, :volume]");
  ruby(*engine, "Save.delete(:name)");
  CHECK(ruby(*engine, "Save[:name].inspect") == "nil");
  ruby(*engine, "Save.clear");
  CHECK(ruby(*engine, "Save.keys.inspect") == "[]");
}

TEST_CASE("scripts read the gamepad, and the usual axes and actions follow it") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  ScriptedInputSource& input = engine->scripted_input();
  frames(*engine, 1);
  CHECK(ruby(*engine, "Input.gamepad?") == "false");
  CHECK(ruby(*engine, "Input.gamepad_axis(:left_x)") == "0.0");

  input.set_gamepad_axis(GamepadAxis::LeftX, kStickHalfway);
  input.set_gamepad_axis(GamepadAxis::RightTrigger, 1.0f);
  input.set_gamepad_button(GamepadButton::A, true);
  frames(*engine, 1);
  CHECK(ruby(*engine, "Input.gamepad?") == "true");
  CHECK(ruby(*engine, "[Input.gamepad_axis(:left_x), Input.axis(:horizontal), Input.gamepad_axis(:right_trigger)]") ==
        "[0.5, 0.5, 1.0]");
  CHECK(ruby(*engine,
             "[Input.gamepad_down?(:a), Input.gamepad_pressed?(:a), Input.action?(:jump), "
             "Input.action_pressed?(:jump), Input.gamepad_down?(:b)]") == "[true, true, true, true, false]");
  input.set_gamepad_button(GamepadButton::A, false);
  frames(*engine, 1);
  CHECK(ruby(*engine, "[Input.gamepad_released?(:a), Input.action?(:jump)]") == "[true, false]");
  CHECK(fails(*engine, "Input.gamepad_down?(:turbo)"));
  CHECK(fails(*engine, "Input.gamepad_axis(:middle)"));
}

TEST_CASE("scripts pause, stop and replay the scene's animations by name") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  CHECK(ruby(*engine, "Animation.names.inspect") == "[\"Open\"]");
  CHECK(ruby(*engine, "Animation.duration(:Open)") == "2.0");
  frames(*engine, kHalfSecond);
  CHECK(door_x(*engine) == doctest::Approx(kClipDistance / 4.0f).epsilon(0.01));  // playing by itself
  CHECK(ruby(*engine, "Animation.playing?(:Open)") == "true");

  ruby(*engine, "Animation.pause(:Open)");
  frames(*engine, kHalfSecond);
  CHECK(door_x(*engine) == doctest::Approx(kClipDistance / 4.0f).epsilon(0.01));
  CHECK(ruby(*engine, "Animation.playing?('Open')") == "false");

  // Once, from the beginning: it ends on its last pose and stays there.
  ruby(*engine, "Animation.play(:Open, loop: false, restart: true)");
  frames(*engine, kFramesPerSecond * 3);
  CHECK(door_x(*engine) == doctest::Approx(kClipDistance));
  CHECK(ruby(*engine, "[Animation.playing?(:Open), Animation.time(:Open)]") == "[false, 2.0]");

  // Backwards and twice as fast: closed again after a second.
  ruby(*engine, "Animation.play(:Open, loop: false, speed: -2.0)");
  frames(*engine, kFramesPerSecond + 1);
  CHECK(door_x(*engine) == doctest::Approx(0.0f));

  // Stopped, the door is the script's to move.
  ruby(*engine, "Animation.stop(:Open); Scene.find('Door').transform.position = Vector3.new(-4, 0, 0)");
  frames(*engine, kHalfSecond);
  CHECK(door_x(*engine) == doctest::Approx(-4.0f));
  ruby(*engine, "Animation.seek(:Open, 1.0)");
  frames(*engine, 1);
  CHECK(door_x(*engine) == doctest::Approx(kClipDistance / 2.0f));
  CHECK(fails(*engine, "Animation.play(:Dance)"));
  CHECK(fails(*engine, "Animation.duration(:Dance)"));
}

TEST_CASE("an object plays a clip of its own, fading into it, with another mixed over it") {
  const TempSave temp;
  SceneDesc scene = small_scene();
  // A second clip for the door: "Ajar" holds it at a fixed place.
  AnimationChannel hold;
  hold.node = 2;
  hold.times = {0.0f, kClipSeconds};
  hold.values = {kAjar, 0, 0, kAjar, 0, 0};
  AnimationDesc ajar;
  ajar.name = "Ajar";
  ajar.duration = kClipSeconds;
  ajar.channels = {hold};
  scene.animations.push_back(ajar);
  auto engine = engine_for(scene, temp.file);
  ruby(*engine,
       "Animation.stop(:Ajar); Animation.pause(:Open); $door = Scene.find('Door')");  // the scene holds it shut
  frames(*engine, 1);
  CHECK(door_x(*engine) == doctest::Approx(0.0f));
  CHECK(ruby(*engine, "$door.animation.clip.inspect") == "nil");

  ruby(*engine, "$door.animation.play(:Ajar, fade: 1.0)");
  frames(*engine, kHalfSecond);
  CHECK(door_x(*engine) == doctest::Approx(kAjar / 2.0f).epsilon(0.05));  // halfway there
  frames(*engine, kFramesPerSecond);
  CHECK(door_x(*engine) == doctest::Approx(kAjar));
  CHECK(ruby(*engine, "[$door.animation.clip, $door.animation.playing?]") == "[\"Ajar\", true]");

  ruby(*engine, "$door.animation.mix(:Open, weight: 0.5, speed: 0.0)");  // Open's first pose: x = 0
  frames(*engine, 1);
  CHECK(door_x(*engine) == doctest::Approx(kAjar / 2.0f));
  ruby(*engine, "$door.animation.unmix; $door.animation.pause");
  frames(*engine, 1);
  CHECK(door_x(*engine) == doctest::Approx(kAjar));
  CHECK(ruby(*engine, "$door.animation.playing?") == "false");

  ruby(*engine, "$door.animation.follow_scene");
  frames(*engine, 1);
  CHECK(door_x(*engine) == doctest::Approx(0.0f));
  CHECK(fails(*engine, "$door.animation.play(:Dance)"));
  CHECK(fails(*engine, "$door.animation.mix(:Dance)"));
  // A behaviour reaches its object's animation directly.
  ruby(*engine, "class Opener < Behaviour; def start = animation.play(:Ajar); end; $door.add_component(Opener)");
  frames(*engine, 2);
  CHECK(door_x(*engine) == doctest::Approx(kAjar));
}

TEST_CASE("scripts let out bursts of particles, which fly for a while and are gone") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  frames(*engine, 1);
  CHECK(ruby(*engine, "Particles.count") == "0");
  CHECK(ruby(*engine, "Particles.burst(Vector3.new(1, 2, 3))") == "12");
  CHECK(ruby(*engine,
             "Particles.burst([0, 0, 0], count: 30, color: :gold, glow: true, life: 0.2, size: 0.3, "
             "speed: 5, spread: 20, direction: Vector3.forward, gravity: 0, alpha: 128)") == "30");
  CHECK(ruby(*engine, "Particles.count") == "42");
  const std::vector<Particle>& particles = engine->particles().particles();
  REQUIRE(particles.size() == 42);
  CHECK_FALSE(particles.front().glow);
  CHECK(particles.front().position.y == doctest::Approx(2.0f));
  const Particle& spark = particles.back();
  CHECK(spark.glow);
  CHECK(spark.color.b == doctest::Approx(0.0f));  // gold has no blue
  CHECK(spark.color.a == doctest::Approx(128.0f / 255.0f));
  CHECK(spark.velocity.z < 0.0f);  // forward is -z

  frames(*engine, kHalfSecond);  // the short-lived sparks are over, the others still falling
  CHECK(ruby(*engine, "Particles.count") == "12");
  CHECK(engine->particles().particles().front().position.y < 2.0f);
  frames(*engine, kFramesPerSecond);
  CHECK(ruby(*engine, "Particles.count") == "0");

  // With a picture from the game's images/ folder, each one turning.
  ruby(*engine, "Particles.burst(Vector3.zero, count: 3, image: :smoke, spin: 90)");
  CHECK(engine->particles().images() == std::vector<std::string>{"smoke"});
  CHECK(engine->particles().particles().back().image == 0);
  CHECK(engine->particles().particles().back().spin != 0.0f);

  ruby(*engine, "Particles.burst(Vector3.zero); Particles.clear");
  CHECK(ruby(*engine, "Particles.count") == "0");
  CHECK(fails(*engine, "Particles.burst(Vector3.zero, color: :plaid)"));
  CHECK(fails(*engine, "Particles.burst('here')"));
}

TEST_CASE("particles stay put while the game is paused") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  frames(*engine, 1);
  ruby(*engine, "Particles.burst(Vector3.zero, life: 0.3); Time.scale = 0");
  frames(*engine, kFramesPerSecond);
  CHECK(ruby(*engine, "Particles.count") == "12");
  CHECK(engine->particles().particles().front().age == doctest::Approx(0.0f));
}

TEST_CASE("scripts read what is typed, and a text field keeps it") {
  const TempSave temp;
  const SceneDesc scene = small_scene();
  auto engine = engine_for(scene, temp.file);
  ScriptedInputSource& input = engine->scripted_input();
  const std::string field = "$name = UI.text_field($name, x: 10, y: 10, width: 300, height: 40, max_length: 6)";
  ruby(*engine, "$name = ''");
  input.type_text("Ana");
  frames(*engine, 1);
  CHECK(ruby(*engine, "Input.text") == "Ana");
  CHECK(ruby(*engine, field) == "Ana");
  input.type_text("");
  frames(*engine, 1);
  CHECK(ruby(*engine, "Input.text") == "");
  CHECK(ruby(*engine, field) == "Ana");
  CHECK(engine->hud().size() == 2);  // the box and its text
  input.set_key(Key::Backspace, true);
  frames(*engine, 1);
  CHECK(ruby(*engine, field) == "An");
  input.set_key(Key::Backspace, false);
  input.type_text("tonia");  // more than fits
  frames(*engine, 1);
  CHECK(ruby(*engine, field) == "Antoni");
}
