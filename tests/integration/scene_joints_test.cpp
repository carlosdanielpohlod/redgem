// Joints a scene's objects come with (their `joint` properties, the editor's Joint card), played in
// a headless game with a fixed step: made as the scene loads, read by scripts, kept by copies.

#include <doctest/doctest.h>

#include <cmath>
#include <string>

#include "physics_game.hpp"
#include "scene/props.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr float kTolerance = 0.05f;
constexpr float kLidLimit = -45.0f;  // degrees: as far as the lid may fall
constexpr float kHingeY = 3.0f;
constexpr float kLidHalfWidth = 1.0f;
constexpr const char* kPlainGame = R"({"title": "Scene joints"})";

PropertyMap dynamic_box() { return {{props::kBody, std::string(props::kBodyDynamic)}}; }

// A lid 2 m wide hinged at its left edge on an axis along z, at (0, 3, 0): gravity swings its right
// side down, until its low limit stops it.
NodeDesc lid() {
  PropertyMap joint = dynamic_box();
  joint[props::kJoint] = std::string(props::kJointHinge);
  joint[props::kJointAnchor[0]] = -0.5;  // its left edge (its scale stretches the point)
  joint[props::kJointAxis[1]] = 0.0;
  joint[props::kJointAxis[2]] = 1.0;
  joint[props::kJointLow] = static_cast<double>(kLidLimit);
  joint[props::kJointHigh] = 0.0;
  NodeDesc desc = node("Lid", {kLidHalfWidth, kHingeY, 0}, {2, 0.1f, 1}, joint);
  desc.stable_id = "lid";
  return desc;
}

// A ball hung on a rope from a fixed hook object 2 m above it.
SceneDesc hook_and_ball() {
  NodeDesc hook = node("Hook", {5, 6, 0}, {0.2f, 0.2f, 0.2f}, solid());
  hook.stable_id = "hook";
  PropertyMap rope = dynamic_box();
  rope[props::kJoint] = std::string(props::kJointDistance);
  rope[props::kJointTo] = std::string("hook");
  NodeDesc ball = node("Ball", {5, 4, 0}, {0.5f, 0.5f, 0.5f}, rope);
  ball.stable_id = "ball";
  return scene_with({hook, ball});
}

}  // namespace

TEST_CASE("a hinge set up on a scene's object swings within its limits, and scripts read it") {
  const TempGame game(kPlainGame);
  const SceneDesc scene = scene_with({lid()});
  auto engine = engine_for(game, scene);
  CHECK(ruby(*engine, "Scene.find('Lid').joint.inspect") == "#<Joint hinge Lid-world>");
  CHECK(ruby(*engine, "Scene.find('Ground').joint.inspect") == "nil");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "Scene.find('Lid').joint.angle") == doctest::Approx(kLidLimit).epsilon(kTolerance));
  // Its middle is a meter out from the hinge, turned down by the limit, and no further.
  const float drop = std::sin(degrees_to_radians(-kLidLimit)) * kLidHalfWidth;
  CHECK(number(*engine, "Scene.find('Lid').transform.position.y") ==
        doctest::Approx(kHingeY - drop).epsilon(kTolerance));
  ruby(*engine, "Scene.find('Lid').joint.remove");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "Scene.find('Lid').transform.position.y") < 1.0f);  // let go, it fell
}

TEST_CASE("a scene's joint holds an object to another one, and a spawned copy to its own copy") {
  const TempGame game(kPlainGame);
  SceneDesc scene = hook_and_ball();
  // A hook and ball together, the hook kinematic: spawned as one, the copy's rope holds the copy's hook.
  NodeDesc pair = node("Pair", {-5, 0, 0}, {1, 1, 1}, {});
  pair.stable_id = "pair";
  NodeDesc hook = node("PairHook", {0, 6, 0}, {0.2f, 0.2f, 0.2f}, moving(props::kBodyKinematic));
  hook.stable_id = "pair_hook";
  hook.parent = static_cast<int>(scene.nodes.size());
  PropertyMap rope = dynamic_box();
  rope[props::kJoint] = std::string(props::kJointDistance);
  rope[props::kJointTo] = std::string("pair_hook");
  NodeDesc ball = node("PairBall", {0, 4, 0}, {0.5f, 0.5f, 0.5f}, rope);
  ball.parent = hook.parent;
  pair.children = {hook.parent + 1, hook.parent + 2};
  scene.nodes.push_back(pair);
  scene.nodes.push_back(hook);
  scene.nodes.push_back(ball);
  auto engine = engine_for(game, scene);
  CHECK(ruby(*engine, "Scene.find('Ball').joint.other.name") == "Hook");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "Scene.find('Ball').transform.position.y") == doctest::Approx(4.0f).epsilon(kTolerance));

  ruby(*engine, "$copy = Scene.spawn(Scene.find('Pair'), position: Vector3.new(20, 0, 0))");
  ruby(*engine, "$ball = $copy.children.find { |child| child.name == 'PairBall' }");
  CHECK(ruby(*engine, "$ball.joint.other == $copy.children.find { |child| child.name == 'PairHook' }") == "true");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "$ball.transform.position.x") == doctest::Approx(20.0f).epsilon(kTolerance));
  CHECK(number(*engine, "$ball.transform.position.y") == doctest::Approx(4.0f).epsilon(kTolerance));
}

TEST_CASE("a scene's joint that cannot be made is said, and the scene still plays") {
  test::LogCapture log;
  const TempGame game(kPlainGame);
  PropertyMap lost = dynamic_box();
  lost[props::kJoint] = std::string(props::kJointFixed);
  lost[props::kJointTo] = std::string("nowhere");
  PropertyMap unknown = dynamic_box();
  unknown[props::kJoint] = std::string("rope");
  const SceneDesc scene = scene_with({node("Lost", {0, 3, 0}, {1, 1, 1}, lost),
                                      node("Odd", {3, 3, 0}, {1, 1, 1}, unknown)});
  auto engine = engine_for(game, scene);
  CHECK(log.contains("is not in the scene"));
  CHECK(log.contains("unknown joint 'rope'"));
  CHECK(ruby(*engine, "[Scene.find('Lost').joint, Scene.find('Odd').joint].inspect") == "[nil, nil]");
  frames(*engine, kOneSecond);
  CHECK(number(*engine, "Scene.find('Lost').transform.position.y") < 2.0f);  // nothing holds it
}
