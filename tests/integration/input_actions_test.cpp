// Scripts declare actions and axes, players bind them anew, and a second gamepad plays too.

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

constexpr std::size_t kSecondPad = 1;
constexpr float kStickHalfway = 0.5f;

// The files of the test's own, removed when the test ends.
struct TempFiles {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-input-actions-test";
  std::filesystem::path save = dir / "save.json";
  std::filesystem::path bindings = dir / "bindings.json";
  TempFiles() { std::filesystem::remove_all(dir); }
  ~TempFiles() { std::filesystem::remove_all(dir); }
  TempFiles(const TempFiles&) = delete;
  TempFiles& operator=(const TempFiles&) = delete;
};

SceneDesc one_camera() {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  desc.nodes = {camera};
  return desc;
}

std::unique_ptr<Engine> engine_for(const SceneDesc& scene, const TempFiles& files) {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  options.scene = &scene;
  options.save_file = files.save;
  options.bindings_file = files.bindings;
  return Engine::create(options, scene);
}

void frame(Engine& engine) { engine.frame(config::kFixedDeltaSeconds); }

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

bool fails(Engine& engine, const std::string& code) { return !engine.scripts().eval(code, "bad.rb").ok; }

}  // namespace

TEST_CASE("scripts declare their own actions and axes, next to the built-in ones") {
  const TempFiles files;
  const SceneDesc scene = one_camera();
  auto engine = engine_for(scene, files);
  ScriptedInputSource& input = engine->scripted_input();
  ruby(*engine, "Input.define_action(:dash, keys: [:e], buttons: [:b])");
  ruby(*engine,
       "Input.define_axis(:climb, negative: ['key:f'], positive: ['key:r'], pad_axis: :right_y, invert: true)");
  CHECK(ruby(*engine, "Input.actions.include?(:dash) && Input.actions.include?(:jump)") == "true");
  CHECK(ruby(*engine, "Input.axes.sort.inspect") == "[:climb, :horizontal, :vertical]");
  CHECK(ruby(*engine, "Input.bindings(:dash).inspect") == R"(["key:e", "pad:b"])");
  input.set_key(Key::E, true);
  input.set_key(Key::R, true);
  frame(*engine);
  CHECK(ruby(*engine, "[Input.action?(:dash), Input.action_pressed?(:dash), Input.axis(:climb)].inspect") ==
        "[true, true, 1.0]");
  input.set_key(Key::E, false);
  frame(*engine);
  CHECK(ruby(*engine, "Input.action_released?(:dash)") == "true");
  CHECK(fails(*engine, "Input.define_action(:dash, keys: [:hyper])"));
  CHECK(fails(*engine, "Input.define_axis(:climb, pad_axis: :middle)"));
}

TEST_CASE("scripts bind a player's actions anew, and the players' bindings are kept for the next run") {
  const TempFiles files;
  const SceneDesc scene = one_camera();
  {
    auto engine = engine_for(scene, files);
    ruby(*engine, "Input.unbind(:jump, key: :space)");
    ruby(*engine, "Input.bind(:jump, key: :k)");
    ruby(*engine, "Input.bind(:horizontal, 'key:j', side: :negative)");
    ruby(*engine, "Input.bind(:fire, button: :y, pad: 1)");
    CHECK(ruby(*engine, "Input.bindings(:jump).inspect") == R"(["key:k", "pad:a"])");
    CHECK(ruby(*engine, "Input.bindings(:horizontal)[:negative].inspect") == R"(["key:a", "key:left", "key:j"])");
    CHECK(fails(*engine, "Input.bind(:horizontal, key: :j)"));       // which way?
    CHECK(fails(*engine, "Input.bind(:jump, key: :j, side: :up)"));  // no such side
    CHECK(fails(*engine, "Input.bind(:fly, key: :j)"));
    CHECK(fails(*engine, "Input.bind(:jump)"));
    CHECK(fails(*engine, "Input.bind(:jump, 'mouse:left')"));
    CHECK(fails(*engine, "Input.bind(:jump, key: :k, pad: 4)"));
  }
  REQUIRE(std::filesystem::exists(files.bindings));
  auto engine = engine_for(scene, files);
  CHECK(ruby(*engine, "Input.bindings(:jump).inspect") == R"(["key:k", "pad:a"])");
  CHECK(ruby(*engine, "Input.bindings(:fire, pad: 1).inspect") == R"(["pad:x", "pad:y"])");
  CHECK(ruby(*engine, "Input.bindings(:fire).inspect") == R"(["key:left_control", "key:enter", "pad:x"])");
  ruby(*engine, "Input.reset_bindings(:jump)");
  CHECK(ruby(*engine, "Input.bindings(:jump).inspect") == R"(["key:space", "pad:a"])");
  ruby(*engine, "Input.reset_bindings");
  CHECK(ruby(*engine, "Input.bindings(:fire, pad: 1).inspect") == R"(["pad:x"])");
  engine.reset();  // (what it reset is written as it ends)
  auto after_reset = engine_for(scene, files);
  CHECK(ruby(*after_reset, "Input.bindings(:horizontal)[:negative].inspect") == R"(["key:a", "key:left"])");
}

TEST_CASE("each gamepad is a player of its own") {
  const TempFiles files;
  const SceneDesc scene = one_camera();
  auto engine = engine_for(scene, files);
  ScriptedInputSource& input = engine->scripted_input();
  frame(*engine);
  CHECK(ruby(*engine, "Input.pads.inspect") == "[]");
  input.set_gamepad_button(GamepadButton::A, true, kSecondPad);
  input.set_gamepad_axis(GamepadAxis::LeftX, kStickHalfway, kSecondPad);
  frame(*engine);
  CHECK(ruby(*engine, "Input.pads.inspect") == "[1]");
  CHECK(ruby(*engine, "[Input.gamepad?, Input.gamepad?(pad: 1)].inspect") == "[false, true]");
  CHECK(ruby(*engine, "[Input.action?(:jump), Input.action_pressed?(:jump, pad: 1)].inspect") == "[false, true]");
  CHECK(ruby(*engine, "[Input.axis(:horizontal), Input.axis(:horizontal, pad: 1)].inspect") == "[0.0, 0.5]");
  CHECK(ruby(*engine, "[Input.gamepad_down?(:a), Input.gamepad_down?(:a, pad: 1)].inspect") == "[false, true]");
  CHECK(ruby(*engine, "Input.gamepad_axis(:left_x, pad: 1)") == "0.5");
  CHECK(fails(*engine, "Input.action?(:jump, pad: -1)"));
  CHECK(fails(*engine, "Input.gamepad?(pad: 4)"));
}

TEST_CASE("a capture hands the next key or button pressed to its block, once") {
  const TempFiles files;
  const SceneDesc scene = one_camera();
  auto engine = engine_for(scene, files);
  ScriptedInputSource& input = engine->scripted_input();
  ruby(*engine, "$caught = []; Input.capture_next { |binding, pad| $caught << [binding, pad] }");
  CHECK(ruby(*engine, "Input.capturing?") == "true");
  frame(*engine);
  CHECK(ruby(*engine, "$caught.inspect") == "[]");
  input.set_gamepad_button(GamepadButton::Y, true, kSecondPad);
  frame(*engine);
  CHECK(ruby(*engine, "$caught.inspect") == R"([["pad:y", 1]])");
  CHECK(ruby(*engine, "Input.capturing?") == "false");
  ruby(*engine, "Input.capture_next { |binding, pad| $caught << [binding, pad] }");
  input.set_key(Key::K, true);
  frame(*engine);
  input.set_key(Key::L, true);
  frame(*engine);
  CHECK(ruby(*engine, "$caught.last.inspect") == R"(["key:k", nil])");
  CHECK(ruby(*engine, "$caught.size") == "2");
  ruby(*engine, "Input.capture_next { $caught << :late }; Input.__scene_reset");
  CHECK(ruby(*engine, "Input.capturing?") == "false");  // (a scene that is gone takes its capture with it)
  input.set_key(Key::M, true);
  frame(*engine);
  CHECK(ruby(*engine, "$caught.size") == "2");
}

TEST_CASE("the players' bindings are written once, at the frame's end, and only when they changed") {
  const TempFiles files;
  const SceneDesc scene = one_camera();
  auto engine = engine_for(scene, files);
  ruby(*engine, "Input.bind(:jump, key: :space); Input.unbind(:jump, key: :z); Input.reset_bindings");
  frame(*engine);
  CHECK_FALSE(std::filesystem::exists(files.bindings));  // (nothing changed)
  ruby(*engine, "Input.bind(:jump, key: :k); Input.bind(:fire, key: :f)");
  CHECK_FALSE(std::filesystem::exists(files.bindings));  // (not in the middle of the frame)
  frame(*engine);
  REQUIRE(std::filesystem::exists(files.bindings));
  std::filesystem::remove(files.bindings);
  frame(*engine);
  CHECK_FALSE(std::filesystem::exists(files.bindings));  // (written already: not again)
  ruby(*engine, "Input.bind(:run, key: :r)");
  engine.reset();  // (a game that ends before its frame does keeps them all the same)
  CHECK(std::filesystem::exists(files.bindings));
}

TEST_CASE("Escape calls a capture off: the block gets no binding, and nothing else sees it") {
  const TempFiles files;
  const SceneDesc scene = one_camera();
  auto engine = engine_for(scene, files);
  ScriptedInputSource& input = engine->scripted_input();
  ruby(*engine, "$caught = :waiting; Input.capture_next { |binding, pad| $caught = [binding, pad] }");
  input.set_key(Key::Escape, true);
  frame(*engine);
  CHECK(ruby(*engine, "[$caught, Input.pressed?(:escape)].inspect") == "[[nil, nil], false]");
  ruby(*engine, "Input.capture_next(cancel: [:q]) { |binding, _pad| $caught = binding }");
  input.set_key(Key::Escape, false);
  frame(*engine);
  input.set_key(Key::Escape, true);
  frame(*engine);
  CHECK(ruby(*engine, "$caught") == "key:escape");
}

TEST_CASE("bindings are written the same ways everywhere, and nil is the first player") {
  const TempFiles files;
  const SceneDesc scene = one_camera();
  auto engine = engine_for(scene, files);
  ruby(*engine,
       "Input.define_axis(:steer, negative: [:a, 'pad:dpad_left'], positive: {keys: [:d], buttons: [:dpad_right]})");
  CHECK(ruby(*engine, "Input.bindings(:steer).inspect") ==
        R"({negative: ["key:a", "pad:dpad_left"], positive: ["key:d", "pad:dpad_right"]})");
  ruby(*engine, "Input.define_action(:dash, :e, 'pad:b', keys: [:q], buttons: [:y])");
  CHECK(ruby(*engine, "Input.bindings(:dash).inspect") == R"(["key:e", "key:q", "pad:b", "pad:y"])");
  ruby(*engine, "Input.bind(:jump, key: :k, button: :y, pad: nil)");
  CHECK(ruby(*engine, "Input.bindings(:jump, pad: nil).inspect") == R"(["key:space", "key:k", "pad:a", "pad:y"])");
  ruby(*engine, "Input.unbind(:jump, key: :k, button: :y, pad: nil)");
  CHECK(ruby(*engine, "Input.bindings(:jump).inspect") == R"(["key:space", "pad:a"])");
  CHECK(ruby(*engine, "Input.action?(:jump, pad: nil)") == "false");
}
