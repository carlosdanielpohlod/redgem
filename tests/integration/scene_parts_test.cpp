#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "app/scene_bodies.hpp"
#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "physics/physics_config.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr float kDropHeight = 5.0f;
constexpr float kMovedTo = 3.0f;
constexpr int kFallingSteps = 30;
constexpr const char* kBodyProp = "body";

NodeDesc node(const std::string& name, const std::string& body) {
  NodeDesc desc;
  desc.name = name;
  desc.local.position = {0.0f, kDropHeight, 0.0f};
  if (!body.empty()) desc.props[kBodyProp] = body;
  return desc;
}

// An object that falls, one a script carries around, and one with no body at all.
struct World {
  SceneDesc desc;
  Scene scene;
  std::unique_ptr<PhysicsWorld> physics = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  ObjectId falling = kNoObject;
  ObjectId carried = kNoObject;
  ObjectId plain = kNoObject;

  World() {
    desc.nodes = {node("Falling", "dynamic"), node("Carried", "kinematic"), node("Plain", "")};
    scene = Scene::from_desc(desc);
    falling = scene.find("Falling");
    carried = scene.find("Carried");
    plain = scene.find("Plain");
    bodies::create_all({scene, desc}, *physics);
  }

  void step() {
    bodies::push_poses(scene, *physics);
    physics->step(config::kFixedDeltaSeconds);
    bodies::pull_poses(scene, *physics);
  }
};

}  // namespace

TEST_CASE("objects get the body their properties ask for, and none when they ask for none") {
  World world;
  CHECK(world.physics->body_type(world.falling) == BodyType::Dynamic);
  CHECK(world.physics->body_type(world.carried) == BodyType::Kinematic);
  CHECK_FALSE(world.physics->has_object(world.plain));
}

TEST_CASE("after a step a simulated object is where its body went, and a carried body where its object is") {
  World world;
  Transform moved = world.scene.world_transform(world.carried);
  moved.position.x = kMovedTo;
  world.scene.set_world_transform(world.carried, moved);
  for (int i = 0; i < kFallingSteps; ++i) world.step();
  CHECK(world.scene.world_transform(world.falling).position.y < kDropHeight);
  CHECK(world.scene.world_transform(world.carried).position.x == doctest::Approx(kMovedTo));
  CHECK(world.scene.world_transform(world.carried).position.y == doctest::Approx(kDropHeight));
}

TEST_CASE("a simulated body goes where a script puts its object") {
  World world;
  Transform moved = world.scene.world_transform(world.falling);
  moved.position.x = kMovedTo;
  world.scene.set_world_transform(world.falling, moved);
  bodies::follow_write(world.scene, *world.physics, world.falling, TransformWrite::Position);
  world.step();
  CHECK(world.scene.world_transform(world.falling).position.x == doctest::Approx(kMovedTo));
}

TEST_CASE("a game's scenes are found by name, inside its folders only") {
  const std::filesystem::path game = fixture("games/sandbox");
  CHECK(scene_file(game, config::kMainSceneName) == game / config::kSceneFileName);
  CHECK(scene_file(game, "level2") == game / config::kScenesDirName / "level2.glb");
  CHECK(safe_scene_name("level2"));
  CHECK_FALSE(safe_scene_name("../level2"));
  CHECK(read_scene(game, "level2").has_value());
  CHECK_FALSE(read_scene(game, "no_such_level").has_value());
  CHECK_FALSE(read_scene(game, "../sandbox/scene").has_value());
}
