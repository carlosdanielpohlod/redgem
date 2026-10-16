// What the natives do with the arguments scripts hand them: what they read, what they refuse and
// what they say when they do.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "script/script_host.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

// A camera and an object to call things on.
SceneDesc small_scene() {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  NodeDesc host;
  host.name = "Host";
  desc.nodes = {camera, host};
  return desc;
}

std::unique_ptr<Engine> engine_for(const SceneDesc& scene) {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  options.scene = &scene;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(ScriptHost& scripts, const std::string& code) {
  const RubyResult result = scripts.eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(scripts.vm().state(), result.value));
}

// Whether running `code` fails with an error that says `message`.
bool refuses(ScriptHost& scripts, const std::string& code, const std::string& message) {
  const RubyResult result = scripts.eval(code, "refused.rb");
  return !result.ok && result.error.find(message) != std::string::npos;
}

}  // namespace

TEST_CASE("what needs a running game says so to a script run without one") {
  Scene scene;
  ScriptContext context;
  context.scene = &scene;
  ScriptHost scripts(context, fixture("games/sandbox/scripts"));
  REQUIRE(scripts.error_count() == 0);
  CHECK(refuses(scripts, "Animation.names", "Animation is only available while a game runs"));
  CHECK(refuses(scripts, "Particles.count", "Particles are only available while a game runs"));
  CHECK(refuses(scripts, "Save.keys", "Save is only available while a game runs"));
  CHECK(refuses(scripts, "Network.online?", "Network is only available while a game runs"));
  CHECK(refuses(scripts, "Network.searching?", "Network is only available while a game runs"));
  CHECK(refuses(scripts, "Scene.__spawn('Host')", "Scene.spawn only works while the game runs"));
}

TEST_CASE("a name that stands for nothing is refused, by what it was meant to name") {
  const SceneDesc scene = small_scene();
  const auto engine = engine_for(scene);
  ScriptHost& scripts = engine->scripts();
  CHECK(refuses(scripts, "Input.down?(:nope)", "unknown key :nope"));
  CHECK(refuses(scripts, "Input.axis(:nope)", "unknown axis :nope"));
  CHECK(refuses(scripts, "Input.action?(:nope)", "unknown action :nope"));
  CHECK(refuses(scripts, "Input.action_pressed?(:nope)", "unknown action :nope"));
  CHECK(refuses(scripts, "Input.mouse_down?(:nope)", "unknown mouse button :nope (use :left, :right or :middle)"));
  CHECK(refuses(scripts, "Input.gamepad_axis(:nope)", "unknown gamepad axis :nope"));
  CHECK(refuses(scripts, "Input.gamepad_down?(:nope)", "unknown gamepad button :nope"));
  CHECK(refuses(scripts, "Input.down?(5)", "expected a Symbol or String"));
  CHECK(refuses(scripts, "Animation.pause(:nope)", "the scene has no animation called 'nope'"));
  CHECK(refuses(scripts, "Game.load_scene(:nope)", "no scene named nope"));
  // What they name is read when it is known, as a Symbol or as a String.
  CHECK(ruby(scripts, "Input.down?(:space)") == "false");
  CHECK(ruby(scripts, "Input.down?('space')") == "false");
  CHECK(ruby(scripts, "Input.mouse_down?(:left)") == "false");
  CHECK(ruby(scripts, "Input.gamepad_down?(:a)") == "false");
}

TEST_CASE("numbers outside what a setter takes are refused, and the others are handed back") {
  const SceneDesc scene = small_scene();
  const auto engine = engine_for(scene);
  ScriptHost& scripts = engine->scripts();
  // One wording for every range, with what was given.
  CHECK(refuses(scripts, "Time.scale = -1", "Time.scale must be at least 0, not -1"));
  CHECK(refuses(scripts, "Camera.fov = 5", "Camera.fov must be from 10 to 150, not 5"));
  CHECK(refuses(scripts, "Camera.fov = 151", "Camera.fov must be from 10 to 150, not 151"));
  CHECK(refuses(scripts, "Camera.motion_blur = 3", "Camera.motion_blur must be from 0 to 1, not 3"));
  CHECK(refuses(scripts, "Audio.music_pitch = 0", "Audio.music_pitch must be more than 0, not 0"));
  CHECK(refuses(scripts, "Audio.music_volume = -0.5", "Audio.music_volume must be at least 0, not -0.5"));
  CHECK(refuses(scripts, "Audio.volume = -1", "Audio.volume must be at least 0, not -1"));
  CHECK(refuses(scripts, "Network.__search(70000)", "a port is a number from 0 to 65535"));
  CHECK(refuses(scripts, "Network.__search(-1)", "a port is a number from 0 to 65535"));
  CHECK(refuses(scripts, "Save.set(:best, 1.0 / 0)", "Save cannot keep infinity or NaN"));
  CHECK(refuses(scripts, "Network.__emit('ping', 0.0 / 0.0, 0)", "a message cannot carry infinity or NaN"));

  CHECK(ruby(scripts, "Time.send(:scale=, 0.5)") == "0.5");
  CHECK(ruby(scripts, "Time.scale") == "0.5");
  CHECK(ruby(scripts, "Camera.send(:fov=, 90)") == "90.0");
  CHECK(ruby(scripts, "Camera.send(:motion_blur=, 0.25)") == "0.25");
  CHECK(ruby(scripts, "Camera.motion_blur") == "0.25");
  CHECK(ruby(scripts, "Input.send(:mouse_locked=, true)") == "true");
  CHECK(ruby(scripts, "Input.mouse_locked?") == "true");
  CHECK(ruby(scripts, "Input.send(:touch_controls=, false)") == "false");
  CHECK(ruby(scripts, "Input.touch_controls?") == "false");
}

TEST_CASE("no native takes infinity or NaN for a number, whatever the number is for") {
  const SceneDesc scene = small_scene();
  const auto engine = engine_for(scene);
  ScriptHost& scripts = engine->scripts();
  ruby(scripts, "$nan = 0.0 / 0.0; $inf = 1.0 / 0.0; $host = Scene.find('Host')");
  for (const char* code :
       {"Time.scale = $nan", "Camera.fov = $nan", "Camera.motion_blur = $inf", "Audio.volume = $nan",
        "Audio.music_pitch = $inf", "$host.transform.position = [0, $nan, 0]", "$host.transform.scale = [$inf, 1, 1]",
        "$host.transform.rotation = Vector3.new(0, $nan, 0)", "$host.tint = [$nan, 1, 1]",
        "$host.animation_offset = $nan", "Physics.raycast([0, 0, 0], [0, -1, 0], $inf)",
        "Physics.raycast([0, $nan, 0], [0, -1, 0])", "Camera.screen_ray($nan, 0)"}) {
    CHECK_MESSAGE(refuses(scripts, code, "expected a finite number"), code);
  }
  CHECK(ruby(scripts, "$host.transform.position.y") == "0.0");  // nothing refused got through
}

TEST_CASE("a place is a Vector3 or three numbers, and nothing else") {
  const SceneDesc scene = small_scene();
  const auto engine = engine_for(scene);
  ScriptHost& scripts = engine->scripts();
  CHECK(refuses(scripts, "Scene.find('Host').transform.position = 5", "expected a Vector3 or [x, y, z]"));
  CHECK(refuses(scripts, "Scene.find('Host').transform.position = [1, 2]", "expected a Vector3 or [x, y, z]"));
  CHECK(ruby(scripts, "Scene.find('Host').transform.position = [1, 2, 3]; Scene.find('Host').transform.position.y") ==
        "2.0");
  CHECK(ruby(scripts, "Scene.find('Host').transform.send(:position=, Vector3.new(4, 5, 6)).x") == "4.0");
  CHECK(ruby(scripts, "Scene.find('Host').transform.look_at(Scene.find('Camera')).class") == "Transform");
  CHECK(ruby(scripts, "Scene.find(:Host).name") == "Host");
  CHECK(refuses(scripts, "Scene.find(5)", "expected a Symbol or String"));
}

TEST_CASE("an object that was destroyed still says its name, and refuses the rest") {
  const SceneDesc scene = small_scene();
  const auto engine = engine_for(scene);
  ScriptHost& scripts = engine->scripts();
  REQUIRE(ruby(scripts, "$host = Scene.find('Host'); $host.alive?") == "true");
  REQUIRE(ruby(scripts, "$host.destroy; $host.alive?") == "false");  // on its way out
  engine->frame(config::kFixedDeltaSeconds);
  CHECK(ruby(scripts, "$host.name") == "Host");
  CHECK(ruby(scripts, "$host.alive?") == "false");
  CHECK(refuses(scripts, "$host.transform.position", "was destroyed"));
  CHECK(refuses(scripts, "$host.visible = false", "was destroyed"));
  CHECK(refuses(scripts, "$host.transform.rotate([0, 90, 0])", "was destroyed"));
  CHECK(ruby(scripts, "Scene.find('Host').nil?") == "true");
}
