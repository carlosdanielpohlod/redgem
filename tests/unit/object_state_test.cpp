#include <doctest/doctest.h>
#include <limits>
#include <stdexcept>
#include "scene/object_state.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace {
Scene checkpoint_tree() {
  SceneDesc desc;
  NodeDesc root, child;
  root.children = {1};
  child.parent = 0;
  child.local.position = {1, 0, 0};
  desc.nodes = {root, child};
  return Scene::from_desc(desc);
}
}  // namespace

TEST_CASE("object state round trips quaternion poses without Euler conversion") {
  GameObject object;
  object.local.position = {3, 4, 5};
  object.local.rotation = quat_from_euler_degrees({20, 45, 70});
  object.local.scale = {2, 3, 4};
  object.tint = {0.2f, 0.5f, 0.7f};
  object.visible = false;
  const auto restored = read_object_state(object_state_json(capture_object_state(object)));
  CHECK(approx_equal(restored.local.position, object.local.position, test::kTolerance));
  CHECK(approx_equal(rotate(restored.local.rotation, Vec3::up()), object.local.up(), test::kTolerance));
  CHECK(approx_equal(restored.local.scale, object.local.scale, test::kTolerance));
  CHECK_FALSE(restored.visible);
  CHECK(approx_equal(restored.tint, object.tint, test::kTolerance));
}

TEST_CASE("malformed object states reject missing values and invalid quaternion components") {
  auto data = object_state_json(ObjectState{});
  SUBCASE("zero turn") { data["local_rotation"] = {0, 0, 0, 0}; }
  SUBCASE("infinite position") { data["local_position"][0] = std::numeric_limits<double>::infinity(); }
  SUBCASE("oversized position") { data["local_position"][0] = double(kMaxCheckpointComponent) * 2; }
  SUBCASE("wrong vector length") { data["tint"] = {1, 0}; }
  SUBCASE("wrong visibility") { data["visible"] = 1; }
  SUBCASE("missing field") { data.erase("local_scale"); }
  CHECK_THROWS(read_object_state(data));
}

TEST_CASE("a complete hierarchy plan precedes every object write") {
  auto scene = checkpoint_tree();
  auto state = capture_object_state(scene.at(0));
  state.local.position = {10, 0, 0};
  state.local.scale = {2, 2, 2};
  const auto plan = plan_object_restore(scene, 0, state);
  REQUIRE(plan.size() == 2);
  CHECK(approx_equal(plan[1].world.position, Vec3{12, 0, 0}, test::kTolerance));
  CHECK(approx_equal(scene.at(0).local.position, Vec3::zero(), test::kTolerance));
}

TEST_CASE("overflowing descendant poses refuse the plan before mutation") {
  auto scene = checkpoint_tree();
  scene.at(1).local.position.x = kMaxCheckpointComponent;
  auto state = capture_object_state(scene.at(0));
  state.local.scale = {2, 2, 2};
  CHECK_THROWS_AS(plan_object_restore(scene, 0, state), std::invalid_argument);
  CHECK(scene.at(0).local.scale.x == 1);
}

TEST_CASE("checkpoint planning rejects objects on their way out") {
  auto scene = checkpoint_tree();
  const auto state = capture_object_state(scene.at(0));
  scene.request_destroy(1);
  CHECK_THROWS_AS(plan_object_restore(scene, 0, state), std::invalid_argument);
  CHECK(scene.at(0).local.position.x == 0);
}

TEST_CASE("checkpoint hierarchies cannot grow past the command limit") {
  SceneDesc desc;
  desc.nodes.resize(kMaxCheckpointObjects + 1);
  for (std::size_t i = 1; i < desc.nodes.size(); ++i) {
    desc.nodes[0].children.push_back(static_cast<int>(i));
    desc.nodes[i].parent = 0;
  }
  const auto scene = Scene::from_desc(desc);
  CHECK_THROWS_AS(plan_object_restore(scene, 0, ObjectState{}), std::invalid_argument);
}
