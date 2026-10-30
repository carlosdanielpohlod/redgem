#include <doctest/doctest.h>
#include "physics_game.hpp"
#include "scene/scene_ids.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;
namespace {
SceneDesc checkpoint_scene(std::vector<NodeDesc> objects) {
  auto desc = scene_with(std::move(objects));
  assign_scene_ids(desc);
  return desc;
}
SceneDesc parent_and_body() {
  auto desc = checkpoint_scene(
      {node("Parent", {0, 0, 0}, {1, 1, 1}, {}), node("Child", {2, 10, 0}, {1, 1, 1}, moving("dynamic"))});
  desc.nodes[2].children = {3};
  desc.nodes[3].parent = 2;
  return desc;
}
}  // namespace

TEST_CASE("object checkpoints survive Save and restore a pose and appearance after restart") {
  const TempGame game(kShooterGame);
  const auto desc = checkpoint_scene({node("Hero", {0, 10, 0}, {1, 1, 1}, moving("dynamic"))});
  {
    auto engine = engine_for(game, desc);
    ruby(*engine, "$hero = Scene.find('Hero'); $hero.transform.position = [3, 10, 5]");
    ruby(*engine, "$hero.visible = false; $hero.tint = [0.2, 0.3, 0.4]; Save[:hero] = $hero.capture_state");
  }
  auto engine = engine_for(game, desc);
  CHECK(ruby(*engine, "$hero = Scene.find('Hero'); $hero.restore_state(Save[:hero]) == $hero") == "true");
  CHECK(number(*engine, "$hero.transform.position.x") == doctest::Approx(3));
  CHECK(ruby(*engine, "!$hero.visible") == "true");
  CHECK(number(*engine, "$hero.tint.y") == doctest::Approx(0.3));
  CHECK(engine->physics().simulated_pose(engine->scene().find("Hero"))->position.x == doctest::Approx(3));
}

TEST_CASE("restoring a parent teleports its physical descendants and stops their movement") {
  const TempGame game(kShooterGame);
  auto engine = engine_for(game, parent_and_body());
  ruby(*engine,
       "$parent = Scene.find('Parent'); $parent.transform.position = [7, 0, 0]; $state = $parent.capture_state");
  ruby(*engine, "$parent.transform.position = [20, 0, 0]; Scene.find('Child').rigidbody.velocity = [5, 0, 0]");
  ruby(*engine, "$parent.restore_state($state)");
  const auto child = engine->scene().find("Child");
  CHECK(engine->scene().world_transform(child).position.x == doctest::Approx(9));
  CHECK(engine->physics().simulated_pose(child)->position.x == doctest::Approx(9));
  CHECK(engine->physics().velocity(child).x == 0);
  frames(*engine, 1);
  CHECK(engine->scene().world_transform(child).position.x == doctest::Approx(9));
}

TEST_CASE("invalid checkpoint data leaves pose appearance and physics unchanged") {
  const TempGame game(kShooterGame);
  auto engine = engine_for(game, parent_and_body());
  ruby(*engine, "$parent = Scene.find('Parent'); $state = $parent.capture_state");
  ruby(*engine, "$state['visible'] = false; $state['tint'] = [0.2, 0.3, 0.4]");
  SUBCASE("late malformed rotation") {
    ruby(*engine, "$state['local_position'] = [99, 0, 0]; $state['local_rotation'] = [0, 0, 0, 0]");
  }
  SUBCASE("missing field") { ruby(*engine, "$state.delete('tint')"); }
  SUBCASE("wrong scene") { ruby(*engine, "$state['scene'] = 'another'"); }
  SUBCASE("wrong source") { ruby(*engine, "$state['source'] = 'different'"); }
  SUBCASE("wrong parent") { ruby(*engine, "$state['parent'] = 'different'"); }
  SUBCASE("wrong body") { ruby(*engine, "$state['body'] = 'dynamic'"); }
  SUBCASE("wrong version") { ruby(*engine, "$state['schema'] = 2"); }
  SUBCASE("collider scaling") { ruby(*engine, "$state['local_scale'] = [2, 2, 2]"); }
  SUBCASE("unknown extra field") { ruby(*engine, "$state['extra'] = true"); }
  CHECK(ruby(*engine, "begin; $parent.restore_state($state); rescue ArgumentError; 'rejected'; end") == "rejected");
  CHECK(number(*engine, "$parent.transform.position.x") == 0);
  CHECK(ruby(*engine, "$parent.visible") == "true");
  CHECK(number(*engine, "$parent.tint.x") == 1);
  CHECK(engine->physics().simulated_pose(engine->scene().find("Child"))->position.x == doctest::Approx(2));
}

TEST_CASE("copy checkpoints restore compatible explicit copies but never overwrite their source") {
  const TempGame game(kShooterGame);
  auto engine = engine_for(game, parent_and_body());
  ruby(*engine, "$source = Scene.find('Parent'); $copy = Scene.spawn($source)");
  ruby(*engine, "$copy.transform.position = [7, 0, 0]; $state = $copy.capture_state");
  CHECK(ruby(*engine, "begin; $source.restore_state($state); rescue ArgumentError; 'rejected'; end") == "rejected");
  ruby(*engine, "$copy.transform.position = [20, 0, 0]; $copy.restore_state($state)");
  CHECK(number(*engine, "$copy.transform.position.x") == doctest::Approx(7));
  CHECK(number(*engine, "$source.transform.position.x") == 0);
}

TEST_CASE("checkpoints reject stale or pending destruction handles without reviving them") {
  const TempGame game(kShooterGame);
  auto engine = engine_for(game, parent_and_body());
  ruby(*engine, "$parent = Scene.find('Parent'); $state = $parent.capture_state; $parent.destroy");
  CHECK(ruby(*engine, "begin; $parent.restore_state($state); rescue RuntimeError; 'gone'; end") == "gone");
  frames(*engine, 1);
  CHECK(ruby(*engine, "begin; $parent.restore_state($state); rescue RuntimeError; 'gone'; end") == "gone");
}

TEST_CASE("checkpoints permit render-only scaling without colliders") {
  const TempGame game(kShooterGame);
  auto engine = engine_for(game, checkpoint_scene({node("Marker", {0, 1, 0}, {1, 1, 1}, {})}));
  ruby(*engine, "$marker = Scene.find('Marker'); $state = $marker.capture_state");
  ruby(*engine, "$state['local_scale'] = [2, 3, 4]; $marker.restore_state($state)");
  CHECK(number(*engine, "$marker.transform.scale.y") == doctest::Approx(3));
}

TEST_CASE("kinematic checkpoint restoration clears translation and spin") {
  const TempGame game(kShooterGame);
  auto engine = engine_for(game, checkpoint_scene({node("Mover", {0, 10, 0}, {1, 1, 1}, moving("kinematic"))}));
  ruby(*engine, "$object = Scene.find('Mover'); $state = $object.capture_state");
  const auto id = engine->scene().find("Mover");
  engine->physics().set_velocity(id, {5, 0, 0});
  engine->physics().set_angular_velocity(id, {0, 2, 0});
  ruby(*engine, "$object.restore_state($state)");
  CHECK(engine->physics().velocity(id).x == 0);
  CHECK(engine->physics().angular_velocity(id).y == 0);
}
