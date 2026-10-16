// Who an object is: serials, which slots are used again after a destruction, and what is
// forgotten about an object once it is gone.

#include <doctest/doctest.h>

#include <algorithm>

#include "scene/scene.hpp"

using namespace redgem;

namespace {

// A prefab with one child, and a plain object after it.
SceneDesc spawnable() {
  SceneDesc desc;
  NodeDesc enemy;
  enemy.name = "Enemy";
  enemy.children = {1};
  enemy.props = {{"prefab", true}};
  NodeDesc gun;
  gun.name = "Gun";
  gun.parent = 0;
  NodeDesc ground;
  ground.name = "Ground";
  desc.nodes = {enemy, gun, ground};
  return desc;
}

// As the engine does at the end of a step: children are torn down before their parents.
void destroy_now(Scene& scene, ObjectId id) {
  scene.request_destroy(id);
  const std::vector<ObjectId> pending = scene.pending_destroy_ids();
  for (auto it = pending.rbegin(); it != pending.rend(); ++it) scene.finalize_destroy(*it);
}

bool contains(const std::vector<ObjectId>& ids, ObjectId id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

}  // namespace

TEST_CASE("every object has a serial no other object ever had, in any scene") {
  Scene first = Scene::from_desc(spawnable());
  Scene second = Scene::from_desc(spawnable());
  CHECK(first.at(0).serial != first.at(2).serial);
  CHECK(first.at(0).serial != second.at(0).serial);  // same slot, another scene: another object
  const ObjectId copy = first.clone(0).front();
  CHECK(first.at(copy).serial != first.at(0).serial);
}

TEST_CASE("a spawned object's slot is used again once it is destroyed, with a new serial") {
  Scene scene = Scene::from_desc(spawnable());
  const std::vector<ObjectId> first = scene.clone(0);
  const auto first_serial = scene.at(first.front()).serial;
  destroy_now(scene, first.front());
  const std::size_t slots = scene.size();

  const std::vector<ObjectId> second = scene.clone(0);
  CHECK(scene.size() == slots);  // a game that keeps spawning and destroying does not grow
  CHECK(contains(first, second.front()));
  CHECK(scene.at(second.front()).serial != first_serial);
  CHECK(scene.is_alive(second.front()));
}

TEST_CASE("the objects of the loaded scene keep their slots: an id below the node count is that node") {
  Scene scene = Scene::from_desc(spawnable());
  destroy_now(scene, 2);
  const ObjectId copy = scene.clone(0).front();
  CHECK(copy >= 3);
  CHECK_FALSE(scene.is_alive(2));
}

TEST_CASE("a destroyed object is no longer one of its parent's children") {
  Scene scene = Scene::from_desc(spawnable());
  const std::vector<ObjectId> copy = scene.clone(0);
  REQUIRE(copy.size() == 2);
  destroy_now(scene, copy[1]);  // the gun alone
  CHECK(scene.at(copy[0]).children.empty());
  scene.clone(0);  // whatever takes the gun's slot is not the enemy's child
  CHECK(scene.at(copy[0]).children.empty());
}

TEST_CASE("a destroyed copy is no longer listed among the copies of its original") {
  Scene scene = Scene::from_desc(spawnable());
  const ObjectId a = scene.clone(0).front();
  const ObjectId b = scene.clone(0).front();
  destroy_now(scene, a);
  CHECK(scene.copies_of(0) == std::vector<ObjectId>{b});
  const ObjectId c = scene.clone(2).front();  // a copy of something else, maybe in a's slot
  CHECK(scene.copies_of(0) == std::vector<ObjectId>{b});
  CHECK(scene.copies_of(2) == std::vector<ObjectId>{c});
}

TEST_CASE("what is pending destruction comes parents first, whatever slots they sit in") {
  Scene scene = Scene::from_desc(spawnable());
  const std::vector<ObjectId> filler = scene.clone(2);  // takes a slot that will be freed first
  const std::vector<ObjectId> first = scene.clone(0);
  destroy_now(scene, filler.front());
  destroy_now(scene, first.front());
  const std::vector<ObjectId> reused = scene.clone(0);  // its gun may sit below the enemy now
  scene.request_destroy(reused.front());
  const std::vector<ObjectId> pending = scene.pending_destroy_ids();
  REQUIRE(pending.size() == 2);
  CHECK(pending.front() == reused.front());
  CHECK(pending.back() == reused.back());
}
