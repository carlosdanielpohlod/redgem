// Parents in a headless game: GameObject#parent, parent= and set_parent (keeping the world place or
// the local one), the cycles refused, Scene.spawn with a name and a parent, and the bodies of what
// moves to another parent.

#include <doctest/doctest.h>

#include <string>

#include "physics_game.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr const char* kGame = R"({"title": "Parents"})";
constexpr float kHalfTurn = 180.0f;
constexpr float kDoubled = 2.0f;
constexpr int kFewFrames = 10;
// A height between the middle of a courier on the ground (0.55 m up) and on a box (0.95 m).
const std::string kOnTheBox = "0.8";

// A holder turned half round and twice as big, a hero beside it, a cart with nothing on it, a
// kinematic lift, a dynamic crate resting on the ground, and a template to spawn.
SceneDesc parenting_scene() {
  NodeDesc holder;
  holder.name = "Holder";
  holder.local.position = {10.0f, 0.0f, 0.0f};
  holder.local.rotation = quat_from_euler_degrees({0.0f, kHalfTurn, 0.0f});
  holder.local.scale = {kDoubled, kDoubled, kDoubled};
  NodeDesc hero;
  hero.name = "Hero";
  hero.local.position = {1.0f, 2.0f, 3.0f};
  NodeDesc cart;
  cart.name = "Cart";
  cart.local.position = {20.0f, 0.0f, 0.0f};
  NodeDesc bullet;
  bullet.name = "Bullet";
  bullet.props = {{"prefab", true}};
  return scene_with({holder, hero, cart, node("Lift", {0, 1, 0}, {1, 1, 1}, moving("kinematic")),
                     node("Crate", {5, 0.5f, 0}, {1, 1, 1}, moving("dynamic")), bullet});
}

bool says(Engine& engine, const std::string& code) { return ruby(engine, code) == "true"; }

bool refused_with(Engine& engine, const std::string& code, const std::string& words) {
  return error_of(engine, code).find(words) != std::string::npos;
}

}  // namespace

TEST_CASE("an object put under another keeps its place in the world, then moves with it") {
  TempGame game(kGame);
  auto engine = engine_for(game, parenting_scene());
  ruby(*engine, "$hero = Scene.find('Hero'); $holder = Scene.find('Holder')");
  CHECK(says(*engine, "$hero.parent.nil?"));

  CHECK(says(*engine, "($hero.parent = $holder) == $holder"));
  CHECK(says(*engine, "$hero.parent == $holder && $holder.children == [$hero]"));
  CHECK(says(*engine, "$hero.transform.position.approx?(Vector3.new(1, 2, 3), 1e-4)"));
  // Half round and twice as big: (1, 2, 3) is (-9, 2, 3) from the holder, (4.5, 1, -1.5) in its axes.
  CHECK(says(*engine, "$hero.transform.local_position.approx?(Vector3.new(4.5, 1, -1.5), 1e-4)"));

  ruby(*engine, "$holder.transform.position = Vector3.new(12, 0, 0)");
  CHECK(says(*engine, "$hero.transform.position.approx?(Vector3.new(3, 2, 3), 1e-4)"));

  ruby(*engine, "$hero.parent = nil");
  CHECK(says(*engine, "$hero.parent.nil? && $holder.children.empty?"));
  CHECK(says(*engine, "$hero.transform.position.approx?(Vector3.new(3, 2, 3), 1e-4)"));
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("keep_world: false keeps the local transform, which the new parent then places") {
  TempGame game(kGame);
  auto engine = engine_for(game, parenting_scene());
  ruby(*engine, "$hero = Scene.find('Hero'); $holder = Scene.find('Holder')");
  CHECK(says(*engine, "$hero.set_parent($holder, keep_world: false) == $holder"));
  CHECK(says(*engine, "$hero.parent == $holder"));
  CHECK(says(*engine, "$hero.transform.local_position.approx?(Vector3.new(1, 2, 3), 1e-4)"));
  // (10, 0, 0) plus (1, 2, 3) doubled and turned half round: (-2, 4, -6).
  CHECK(says(*engine, "$hero.transform.position.approx?(Vector3.new(8, 4, -6), 1e-4)"));

  ruby(*engine, "$hero.set_parent(nil, keep_world: false)");
  CHECK(says(*engine, "$hero.transform.position.approx?(Vector3.new(1, 2, 3), 1e-4)"));
}

TEST_CASE("an object cannot go under itself or under what it carries, and stays where it was") {
  TempGame game(kGame);
  auto engine = engine_for(game, parenting_scene());
  ruby(*engine, "$hero = Scene.find('Hero'); $holder = Scene.find('Holder'); $hero.parent = $holder");
  CHECK(refused_with(*engine, "$hero.parent = $hero", "cannot go under itself"));
  CHECK(refused_with(*engine, "$holder.parent = $hero", "cannot go under itself"));
  CHECK(refused_with(*engine, "$holder.set_parent($hero, keep_world: false)", "cannot go under itself"));
  CHECK(says(*engine, "$holder.parent.nil? && $hero.parent == $holder"));

  CHECK(refused_with(*engine, "$hero.parent = 5", "expects a GameObject"));
  CHECK(refused_with(*engine, "$hero.parent = Scene.find('Bullet')", "is a template"));
  CHECK(refused_with(*engine, "Scene.find('Bullet').parent = $holder", "is a template"));
  CHECK(says(*engine, "$hero.parent == $holder"));
}

TEST_CASE("Scene.spawn names a copy and puts it under a parent") {
  TempGame game(kGame);
  auto engine = engine_for(game, parenting_scene());
  ruby(*engine, "$holder = Scene.find('Holder')");
  ruby(*engine, "$shot = Scene.spawn('Bullet', name: 'Shot', parent: $holder, position: Vector3.new(10, 1, 0))");
  CHECK(ruby(*engine, "$shot.name") == "Shot");
  CHECK(says(*engine, "Scene.find('Shot') == $shot && $shot.parent == $holder"));
  CHECK(says(*engine, "$holder.children.include?($shot)"));
  CHECK(says(*engine, "$shot.transform.position.approx?(Vector3.new(10, 1, 0), 1e-4)"));
  ruby(*engine, "$holder.transform.position = Vector3.new(11, 0, 0)");
  CHECK(says(*engine, "$shot.transform.position.approx?(Vector3.new(11, 1, 0), 1e-4)"));

  CHECK(ruby(*engine, "Scene.spawn('Bullet', name: :Second).name") == "Second");
  CHECK(says(*engine, "s = Scene.spawn('Bullet'); s.name == 'Bullet' && s.parent.nil?"));
  CHECK(refused_with(*engine, "Scene.spawn('Bullet', name: '')", "must not be empty"));
  CHECK(refused_with(*engine, "Scene.spawn('Bullet', parent: Scene.find('Bullet'))", "is a template"));
  CHECK(refused_with(*engine, "Scene.spawn('Bullet', parent: 'Holder')", "expects a GameObject"));
  CHECK(refused_with(*engine, "Scene.spawn('Bullet', colour: 1)", "colour"));
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("a kinematic body under a parent goes where the parent takes it") {
  TempGame game(kGame);
  auto engine = engine_for(game, parenting_scene());
  const std::string lift_below =
      "(hit = Physics.raycast([-10, 5, 0], [0, -1, 0])) && hit[:object] == Scene.find('Lift')";
  ruby(*engine, "$cart = Scene.find('Cart'); Scene.find('Lift').parent = $cart");
  frames(*engine, 1);
  CHECK_FALSE(says(*engine, lift_below));
  ruby(*engine, "$cart.transform.position = Vector3.new(10, 0, 0)");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "Scene.find('Lift').transform.position.approx?(Vector3.new(-10, 1, 0), 1e-4)"));
  CHECK(says(*engine, lift_below));
}

// A courier with a box in hand, touching its capsule: a scene of the two, the box under a hand.
SceneDesc courier_with_box() {
  NodeDesc hero = node("Courier", {0, 1, 20}, {1, 1, 1}, {{"body", std::string("character")}});
  hero.children = {3};
  NodeDesc hand = node("Hand", {0, 0.2f, -0.6f}, {1, 1, 1}, {});  // (no body: the box under it rides too)
  hand.parent = 2;                                                // (after the camera and the ground)
  hand.children = {4};
  NodeDesc box = node("Box", {0, 0, 0}, {0.4f, 0.4f, 0.4f}, moving("kinematic"));
  box.parent = 3;
  return scene_with({hero, hand, box});
}

TEST_CASE("a character is not pushed by the kinematic body it carries") {
  // The box touching the courier used to push it, and the courier, moving, took the box on:
  // standing still it drifted away, and teleported it was shoved meters past where it was put.
  TempGame game(kGame);
  auto engine = engine_for(game, courier_with_box());
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "(Scene.find('Courier').transform.position.flat - Vector3.new(0, 0, 20)).length < 0.1"));
  ruby(*engine, "Scene.find('Courier').transform.position = Vector3.new(0, 1, -20)");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "(Scene.find('Courier').transform.position.flat - Vector3.new(0, 0, -20)).length < 0.1"));
  CHECK(says(*engine, "(Scene.find('Box').transform.position.flat - Vector3.new(0, 0, -20.6)).length < 0.1"));
}

// A courier standing on a loose box, with an empty hand.
SceneDesc courier_on_box() {
  NodeDesc hero = node("Courier", {0, 1.6f, 20}, {1, 1, 1}, {{"body", std::string("character")}});
  hero.children = {3};
  NodeDesc hand = node("Hand", {0, 0.2f, -0.6f}, {1, 1, 1}, {});
  hand.parent = 2;  // (after the camera and the ground)
  return scene_with({hero, hand, node("Box", {0, 0.2f, 20}, {1, 0.4f, 1}, moving("dynamic"))});
}

TEST_CASE("a character that picks up the body it stands on stays on the ground") {
  // The box it stood on, made kinematic and put in its hand, was still its ground for a step: the
  // character took the box's jump up to the hand as its own, rose, lifted the box with it, and
  // flew on (a courier picking up the parcel under its feet went 90 m up).
  TempGame game(kGame);
  auto engine = engine_for(game, courier_on_box());
  frames(*engine, kFewFrames * kFewFrames);
  CHECK(says(*engine, "Scene.find('Courier').transform.position.y > " + kOnTheBox));
  ruby(*engine,
       "box = Scene.find('Box'); box.rigidbody.kinematic = true; box.parent = Scene.find('Hand'); "
       "box.transform.position = Scene.find('Hand').transform.position");
  frames(*engine, kFewFrames * kFewFrames);
  CAPTURE(ruby(*engine, "Scene.find('Courier').transform.position.y"));
  CHECK(says(*engine, "Scene.find('Courier').transform.position.y < " + kOnTheBox));  // back on the ground
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("a wheel stays on its vehicle") {
  NodeDesc car = node("Car", {0, 1, 0}, {2, 0.5f, 4}, moving("vehicle"));
  car.children = {3};
  NodeDesc wheel = node("Wheel", {0.9f, -0.3f, 1.3f}, {0.3f, 0.8f, 0.8f}, {{"wheel", true}});
  wheel.parent = 2;  // (after the camera and the ground)
  TempGame game(kGame);
  auto engine = engine_for(game, scene_with({car, wheel}));
  REQUIRE(says(*engine, "Scene.find('Car').vehicle != nil"));
  CHECK(refused_with(*engine, "Scene.find('Wheel').parent = nil", "wheel of its vehicle"));
  CHECK(says(*engine, "Scene.find('Wheel').parent == Scene.find('Car')"));
}

TEST_CASE("a dynamic body keeps its own motion under a parent, and moves with keep_world: false") {
  TempGame game(kGame);
  auto engine = engine_for(game, parenting_scene());
  ruby(*engine, "$cart = Scene.find('Cart'); $crate = Scene.find('Crate'); $crate.parent = $cart");
  frames(*engine, 1);
  ruby(*engine, "$cart.transform.position = Vector3.new(25, 0, 0)");
  frames(*engine, kFewFrames);
  CHECK(number(*engine, "$crate.transform.position.x") == doctest::Approx(5.0f).epsilon(0.01));
  CHECK(says(*engine, "$crate.parent == $cart"));

  // Its place relative to the cart, (-20, 0.5, 0), is now its place in the world: it is taken there.
  ruby(*engine, "$crate.set_parent(nil, keep_world: false)");
  frames(*engine, kFewFrames);
  CHECK(number(*engine, "$crate.transform.position.x") == doctest::Approx(-20.0f).epsilon(0.01));
  CHECK(engine->scripts().error_count() == 0);
}
