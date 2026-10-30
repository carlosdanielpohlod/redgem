#include <doctest/doctest.h>
#include <algorithm>
#include <utility>
#include "scene/scene.hpp"

using namespace redgem;
namespace {
SceneDesc identified_nodes() {
  SceneDesc desc;
  NodeDesc left, right;
  left.name = right.name = "Same name";
  left.stable_id = "left-node";
  right.stable_id = "right-node";
  desc.nodes = {left, right};
  return desc;
}
}  // namespace

TEST_CASE("scene identities survive renaming and file node reordering") {
  auto desc = identified_nodes();
  auto scene = Scene::from_desc(desc);
  REQUIRE(scene.find_by_stable_id("left-node") == 0);
  std::reverse(desc.nodes.begin(), desc.nodes.end());
  desc.nodes[1].name = "Renamed";
  scene = Scene::from_desc(desc);
  CHECK(scene.find_by_stable_id("left-node") == 1);
  CHECK(scene.at(1).name == "Renamed");
  CHECK(scene.find_by_stable_id("right-node") == 0);
}

TEST_CASE("copies and reused runtime slots cannot impersonate a file identity") {
  auto scene = Scene::from_desc(identified_nodes());
  const auto copy = scene.clone(0).front();
  const auto second = scene.clone(copy).front();
  CHECK(scene.at(copy).stable_id.empty());
  CHECK(scene.at(second).stable_id.empty());
  scene.request_destroy(copy);
  scene.finalize_destroy(copy);
  CHECK(scene.clone(1).front() == copy);
  CHECK(scene.at(copy).stable_id.empty());
  CHECK(scene.find_by_stable_id("left-node") == 0);
}

TEST_CASE("deleted originals never resolve through their surviving copies") {
  auto scene = Scene::from_desc(identified_nodes());
  const auto copy = scene.clone(0).front();
  scene.request_destroy(0);
  scene.finalize_destroy(0);
  CHECK(scene.is_alive(copy));
  CHECK(scene.find_by_stable_id("left-node") == kNoObject);
  CHECK(scene.find_by_stable_id("right-node") == 1);
  CHECK(scene.find_by_stable_id("") == kNoObject);
  CHECK(scene.find_by_stable_id("missing") == kNoObject);
}

TEST_CASE("refreshing a loaded object keeps the scene file identity current") {
  auto desc = identified_nodes();
  auto scene = Scene::from_desc(desc);
  const auto handle = scene.handle(0);
  desc.nodes[0].stable_id = "replacement-id";
  scene.refresh_from_desc(desc, 0);
  CHECK(scene.resolve(handle) == 0);
  CHECK(scene.find_by_stable_id("left-node") == kNoObject);
  CHECK(scene.find_by_stable_id("replacement-id") == 0);
}

TEST_CASE("copies clear identities throughout a cloned hierarchy") {
  auto desc = identified_nodes();
  desc.nodes[0].children = {1};
  desc.nodes[1].parent = 0;
  auto scene = Scene::from_desc(desc);
  const auto copied = scene.clone(0);
  REQUIRE(copied.size() == 2);
  for (const auto id : copied) CHECK(scene.at(id).stable_id.empty());
  CHECK(scene.find_by_stable_id("right-node") == 1);
}

TEST_CASE("anonymous in-memory nodes cannot resolve through an empty identity") {
  auto desc = identified_nodes();
  desc.nodes[0].stable_id.clear();
  const auto scene = Scene::from_desc(desc);
  CHECK(scene.is_alive(0));
  CHECK(scene.at(0).stable_id.empty());
  CHECK(scene.find_by_stable_id("") == kNoObject);
  CHECK(scene.find_by_stable_id("right-node") == 1);
}

TEST_CASE("identity lookup stays safe in a scene whose contents were moved") {
  auto original = Scene::from_desc(identified_nodes());
  const auto moved = std::move(original);
  CHECK(moved.find_by_stable_id("left-node") == 0);
  CHECK(original.find_by_stable_id("left-node") == kNoObject);
}
