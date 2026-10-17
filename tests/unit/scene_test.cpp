#include <doctest/doctest.h>

#include <cmath>
#include <stdexcept>

#include "scene/scene.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr int kMadeUpIndex = -7;  // what a broken file may hold

// The objects a skin's bone handles stand for.
std::vector<ObjectId> bone_ids(const Scene& scene, ObjectId skinned) {
  std::vector<ObjectId> ids;
  for (const ObjectHandle& bone : scene.at(skinned).joints) ids.push_back(scene.resolve(bone));
  return ids;
}

SceneDesc family() {
  SceneDesc desc;
  NodeDesc parent;
  parent.name = "Parent";
  parent.local.position = {0, 5, 0};
  parent.children = {1};
  parent.props = {{"tag", std::string("enemy")}};
  NodeDesc child;
  child.name = "Child";
  child.parent = 0;
  child.local.position = {1, 0, 0};
  child.props = {{"visible", false}};
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  desc.nodes = {parent, child, camera};
  return desc;
}

}  // namespace

TEST_CASE("scene objects keep names, tags and visibility from props") {
  const Scene scene = Scene::from_desc(family());
  CHECK(scene.size() == 3);
  CHECK(scene.find("Child") == 1);
  CHECK(scene.find("Nope") == kNoObject);
  CHECK(scene.find_all_by_tag("enemy") == std::vector<ObjectId>{0});
  CHECK_FALSE(scene.at(1).visible);
  CHECK(scene.main_camera() == 2);
}

TEST_CASE("world transforms follow the hierarchy") {
  Scene scene = Scene::from_desc(family());
  CHECK(approx_equal(scene.world_transform(1).position, Vec3{1, 5, 0}, kTolerance));
  scene.set_world_position(1, {0, 0, 0});
  CHECK(approx_equal(scene.at(1).local.position, Vec3{0, -5, 0}, kTolerance));
  scene.at(0).local.position = {0, 10, 0};
  CHECK(approx_equal(scene.world_transform(1).position, Vec3{0, 5, 0}, kTolerance));
}

TEST_CASE("destroying a parent destroys its children in two phases") {
  Scene scene = Scene::from_desc(family());
  scene.request_destroy(0);
  CHECK(scene.is_alive(0));  // still usable by on_destroy callbacks
  CHECK(scene.pending_destroy_ids() == std::vector<ObjectId>{0, 1});
  for (const ObjectId id : scene.pending_destroy_ids()) scene.finalize_destroy(id);
  CHECK_FALSE(scene.is_alive(0));
  CHECK_FALSE(scene.is_alive(1));
  CHECK(scene.find("Parent") == kNoObject);
  CHECK(scene.alive_ids() == std::vector<ObjectId>{2});
  scene.request_destroy(0);  // destroying twice is harmless
  CHECK(scene.pending_destroy_ids().empty());
}

TEST_CASE("invalid ids throw") { CHECK_THROWS_AS(Scene().at(0), std::out_of_range); }

TEST_CASE("an object's tint comes from its `tint` property and defaults to none") {
  SceneDesc desc;
  NodeDesc painted;
  painted.name = "Painted";
  painted.props["tint"] = std::string("#ff0000");
  NodeDesc plain;
  plain.name = "Plain";
  NodeDesc broken;
  broken.name = "Broken";
  broken.props["tint"] = std::string("red");  // not a color: ignored
  desc.nodes = {painted, plain, broken};
  const Scene scene = Scene::from_desc(desc);
  CHECK(scene.at(0).tint.x == doctest::Approx(1.0f));
  CHECK(scene.at(0).tint.y == doctest::Approx(0.0f));
  CHECK(scene.at(1).tint.y == doctest::Approx(1.0f));
  CHECK(scene.at(2).tint.z == doctest::Approx(1.0f));
}

TEST_CASE("a copied character is deformed by its own bones, not the template's") {
  SceneDesc desc;
  NodeDesc root;
  root.name = "Walker";
  root.children = {1, 2};
  NodeDesc bone;
  bone.name = "Bone";
  bone.parent = 0;
  NodeDesc body;
  body.name = "Body";
  body.parent = 0;
  body.skin = 0;
  NodeDesc outside;
  outside.name = "SharedBone";  // a joint that is not part of the copied subtree
  desc.nodes = {root, bone, body, outside};
  SkinDesc skin;
  skin.joints = {1, 3};
  skin.inverse_bind.resize(2);
  desc.skins = {skin};

  Scene scene = Scene::from_desc(desc);
  CHECK(bone_ids(scene, 2) == std::vector<ObjectId>{1, 3});
  const std::vector<ObjectId> copies = scene.clone(0);
  REQUIRE(copies.size() == 3);
  const ObjectId copied_bone = scene.at(copies[0]).children[0];
  const ObjectId copied_body = scene.at(copies[0]).children[1];
  CHECK(scene.at(copied_body).skin == 0);
  CHECK(bone_ids(scene, copied_body) == std::vector<ObjectId>{copied_bone, 3});
  CHECK(bone_ids(scene, 2) == std::vector<ObjectId>{1, 3});  // the template is untouched
}

TEST_CASE("objects cast shadows unless their props say not, and copies keep it") {
  SceneDesc desc;
  NodeDesc dome;
  dome.name = "SkyDome";
  dome.props = {{"cast_shadow", false}};
  NodeDesc plain;
  plain.name = "Plain";
  desc.nodes = {dome, plain};
  Scene scene = Scene::from_desc(desc);
  CHECK_FALSE(scene.at(0).cast_shadow);
  CHECK(scene.at(1).cast_shadow);
  CHECK_FALSE(scene.at(scene.clone(0).front()).cast_shadow);
}

TEST_CASE("objects read their range of visible distances from props, and copies keep it") {
  constexpr double kFrom = 15.0;
  constexpr double kTo = 45.0;
  SceneDesc desc;
  NodeDesc level;
  level.name = "CarMid";
  level.props = {{"visible_from", kFrom}, {"visible_to", kTo}};
  NodeDesc plain;
  plain.name = "Plain";
  desc.nodes = {level, plain};
  Scene scene = Scene::from_desc(desc);
  CHECK(scene.at(0).visible_from == doctest::Approx(kFrom));
  CHECK(scene.at(0).visible_to == doctest::Approx(kTo));
  CHECK(scene.at(1).visible_from == doctest::Approx(0.0));
  CHECK(std::isinf(scene.at(1).visible_to));  // no limit unless the scene sets one
  const ObjectId copy = scene.clone(0).front();
  CHECK(scene.at(copy).visible_to == doctest::Approx(kTo));
}

TEST_CASE("an index names one of so many things only from zero up to their count") {
  CHECK(is_index_of(0, 1));
  CHECK(is_index_of(2, 3));
  CHECK_FALSE(is_index_of(3, 3));
  CHECK_FALSE(is_index_of(kNoIndex, 3));
  CHECK_FALSE(is_index_of(kMadeUpIndex, 3));
  CHECK_FALSE(is_index_of(0, 0));
}

TEST_CASE("what is destroyed is no longer found, by name, by tag or as the camera") {
  Scene scene = Scene::from_desc(family());
  const ObjectId parent = scene.find("Parent");
  const ObjectId camera = scene.main_camera();
  REQUIRE(parent != kNoObject);
  REQUIRE(camera == scene.find("Camera"));
  CHECK(scene.find_all_by_tag("enemy") == std::vector<ObjectId>{parent});
  CHECK(scene.pending_destroy_ids().empty());

  scene.request_destroy(camera);
  CHECK(scene.pending_destroy_ids() == std::vector<ObjectId>{camera});
  CHECK(scene.main_camera() == camera);  // still there until the step ends
  scene.finalize_destroy(camera);
  scene.finalize_destroy(parent);
  CHECK(scene.pending_destroy_ids().empty());
  CHECK(scene.main_camera() == kNoObject);
  CHECK(scene.find("Parent") == kNoObject);
  CHECK(scene.find_all_by_tag("enemy").empty());
}

TEST_CASE("what is under a prefab is a prefab too, and prefabs are not found by their tag") {
  SceneDesc desc = family();
  desc.nodes[0].props["prefab"] = true;
  desc.nodes[1].props["tag"] = std::string("enemy");
  const Scene scene = Scene::from_desc(desc);
  CHECK(scene.is_prefab(scene.find("Parent")));
  CHECK(scene.is_prefab(scene.find("Child")));
  CHECK_FALSE(scene.is_prefab(scene.find("Camera")));
  CHECK(scene.find_all_by_tag("enemy").empty());
  CHECK(scene.find("Child") != kNoObject);  // by name, a template is found
}

TEST_CASE("wind sways an object's foliage unless it says not to, or a skeleton moves it") {
  SceneDesc desc;
  NodeDesc tree;
  tree.name = "Tree";
  NodeDesc fence;
  fence.name = "Fence";
  fence.props = {{"sway", false}};
  NodeDesc samurai;
  samurai.name = "Samurai";
  samurai.skin = 0;
  NodeDesc flag;
  flag.name = "Flag";
  flag.skin = 0;
  flag.props = {{"sway", true}};
  desc.nodes = {tree, fence, samurai, flag};
  desc.skins = {SkinDesc{}};
  const Scene scene = Scene::from_desc(desc);
  CHECK(scene.at(0).sways);
  CHECK_FALSE(scene.at(1).sways);
  CHECK_FALSE(scene.at(2).sways);  // a character's hair follows its head
  CHECK(scene.at(3).sways);
}
