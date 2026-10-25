// Joints played through Ruby in a headless game with a fixed step: a door on a hinge, a ball on a
// rope, and how long a joint lasts.

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "physics_game.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr int kFiveSeconds = kOneSecond * 5;
constexpr float kTolerance = 0.05f;

constexpr const char* kPlainGame = R"({"title": "Joints"})";

PropertyMap ball(double mass = 1.0) {
  return {{"body", std::string("dynamic")}, {"collider", std::string("sphere")}, {"mass", mass}};
}

// A door 2 m wide whose left edge is at the origin, and a ball 3 m under a hook at (0, 8, 0).
SceneDesc yard() {
  return scene_with({node("Door", {1, 1.5f, 0}, {2, 2, 0.1f}, moving("dynamic")),
                     node("Ball", {5, 5, 0}, {0.5f, 0.5f, 0.5f}, ball())});
}

}  // namespace

TEST_CASE("a hinged door swings to its limit under its motor, and back") {
  const TempGame game(kPlainGame);
  const SceneDesc scene = yard();
  auto engine = engine_for(game, scene);
  ruby(*engine, R"(
    $door = Physics.joint(:hinge, Scene.find('Door'), nil, anchor: [0, 1.5, 0], axis: Vector3.up,
                          limits: [0, 90], motor_speed: 180, motor_torque: 100)
  )");
  CHECK(ruby(*engine, "$door.kind.inspect") == ":hinge");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "$door.angle") == doctest::Approx(90.0f).epsilon(kTolerance));
  CHECK(number(*engine, "Scene.find('Door').transform.position.z") == doctest::Approx(-1.0f).epsilon(kTolerance));
  ruby(*engine, "$door.motor(-180, 100)");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "$door.angle") == doctest::Approx(0.0f).epsilon(kTolerance));
}

TEST_CASE("a ball on a distance joint hangs from its hook") {
  const TempGame game(kPlainGame);
  const SceneDesc scene = yard();
  auto engine = engine_for(game, scene);
  ruby(*engine, "$rope = Physics.joint(:distance, Scene.find('Ball'), nil, other_anchor: [5, 8, 0])");
  frames(*engine, kFiveSeconds);
  CHECK(number(*engine, "Scene.find('Ball').transform.position.y") == doctest::Approx(5.0f).epsilon(kTolerance));
  CHECK(ruby(*engine, "$rope.alive?") == "true");

  // A spring lets it bounce below its length, then settle there under its weight.
  ruby(*engine,
       "$rope.remove; Physics.joint(:distance, Scene.find('Ball'), nil, other_anchor: [5, 8, 0], "
       "min: 3, max: 3, spring: 2, damping: 1)");
  CHECK(ruby(*engine, "$rope.alive?") == "false");
  frames(*engine, kFiveSeconds);
  CHECK(number(*engine, "Scene.find('Ball').transform.position.y") < 5.0f - kTolerance);
}

TEST_CASE("a joint goes with its object, and lets the other end go") {
  const TempGame game(kPlainGame);
  const SceneDesc scene = yard();
  auto engine = engine_for(game, scene);
  ruby(*engine, R"(
    $glue = Physics.joint(:fixed, Scene.find('Door'), Scene.find('Ball'))
    $hook = Physics.joint(:fixed, Scene.find('Ball'), nil)
  )");
  frames(*engine, kOneSecond);
  CHECK(number(*engine, "Scene.find('Door').transform.position.y") > 1.2f);  // held up, if sagging on its long arm
  ruby(*engine, "Scene.find('Ball').destroy");
  frames(*engine, 1);
  CHECK(ruby(*engine, "[$glue.alive?, $hook.alive?, $glue.broken?].inspect") == "[false, false, false]");
  CHECK(ruby(*engine, "$hook.inspect") == "#<Joint fixed Ball-world (gone)>");
  ruby(*engine, "$hook.remove; $hook.enabled = false");  // nothing to do any more, and no error
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "Scene.find('Door').transform.position.y") == doctest::Approx(1.0f).epsilon(kTolerance));
}

TEST_CASE("joints go when the scene changes") {
  const TempGame game(kPlainGame);
  fs::create_directories(game.dir / "scenes");
  fs::copy_file(test::fixture("games/sandbox/scenes/level2.glb"), game.dir / "scenes" / "level2.glb");
  const SceneDesc scene = yard();
  auto engine = engine_for(game, scene);
  ruby(*engine, "$rope = Physics.joint(:distance, Scene.find('Ball'), nil, other_anchor: [5, 8, 0])");
  ruby(*engine, "Game.load_scene('level2')");
  frames(*engine, 1);
  CHECK(ruby(*engine, "[$rope.alive?, $rope.broken?].inspect") == "[false, false]");
}

TEST_CASE("a joint pulled past its break force breaks; a disabled one lets go") {
  const TempGame game(kPlainGame);
  SceneDesc scene = yard();
  scene.nodes.back().props["mass"] = 10.0;
  auto engine = engine_for(game, scene);
  ruby(*engine, "$rope = Physics.joint(:distance, Scene.find('Ball'), nil, other_anchor: [5, 8, 0])");
  ruby(*engine, "$rope.break_force = 50.0");
  CHECK(number(*engine, "$rope.break_force") == doctest::Approx(50.0f));
  frames(*engine, kOneSecond);
  CHECK(ruby(*engine, "[$rope.alive?, $rope.broken?].inspect") == "[false, true]");

  ruby(*engine, R"(
    $door = Physics.joint(:fixed, Scene.find('Door'), nil)
    $door.enabled = false
  )");
  CHECK(ruby(*engine, "$door.enabled?") == "false");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "Scene.find('Door').transform.position.y") == doctest::Approx(1.0f).epsilon(kTolerance));
}

TEST_CASE("Physics.joint refuses what it cannot make, saying why") {
  const TempGame game(kPlainGame);
  SceneDesc scene = yard();
  scene.nodes.push_back(node("Hero", {-5, 1, 0}, {0.8f, 2, 0.8f}, moving("character")));
  auto engine = engine_for(game, scene);
  ruby(*engine, "$door = Scene.find('Door')");
  const auto refusal = [&](const std::string& code) { return error_of(*engine, code); };
  CHECK(refusal("Physics.joint(:rope, $door)").find("unknown joint 'rope'") != std::string::npos);
  CHECK(refusal("Physics.joint(:hinge, $door, nil, lenght: 2)").find("a hinge joint takes no :lenght") !=
        std::string::npos);
  CHECK(refusal("Physics.joint(:point, $door, nil, limits: [-30, 30])").find("takes no :limits") != std::string::npos);
  CHECK(refusal("Physics.joint(:hinge, $door, nil, motor_speed: 90)").find("needs a motor_torque") !=
        std::string::npos);
  CHECK(refusal("Physics.joint(:fixed, $door).angle").find("only hinges have angle") != std::string::npos);
  CHECK(refusal("Physics.joint(:hinge, $door, nil, limits: [10, 90])").find("low limit") != std::string::npos);
  CHECK(refusal("Physics.joint(:hinge, $door, nil, limits: [0, 270])").find("high limit") != std::string::npos);
  CHECK(refusal("Physics.joint(:hinge, $door, nil, axis: Vector3.zero)").find("axis") != std::string::npos);
  CHECK(refusal("Physics.joint(:point, Scene.find('Hero'))").find("rigid bodies") != std::string::npos);
  CHECK(refusal("Physics.joint(:point, Scene.find('Ground'))").find("dynamic body") != std::string::npos);
  CHECK(refusal("Physics.joint(:distance, $door, nil, min: 3, max: 1)").find("max") != std::string::npos);
  CHECK(refusal("Physics.joint(:fixed, $door, nil, break_force: -1)").find("break force") != std::string::npos);
}
