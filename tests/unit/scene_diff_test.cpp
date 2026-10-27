#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <stdexcept>
#include <utility>

#include "project/scene_diff.hpp"
#include "scene/scene_ids.hpp"

using namespace redgem;
using redgem::tools::scene_difference;

namespace {

NodeDesc object(const std::string& id, const std::string& name) {
  NodeDesc node;
  node.stable_id = id;
  node.name = name;
  return node;
}

SceneDesc objects() {
  SceneDesc scene;
  scene.nodes = {object("one", "One"), object("two", "Two")};
  return scene;
}

MeshDesc triangle(const std::string& name, int material) {
  MeshDesc mesh;
  mesh.name = name;
  PrimitiveDesc primitive;
  primitive.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  primitive.normals = {0, 0, 1, 0, 0, 1, 0, 0, 1};
  primitive.indices = {0, 1, 2};
  primitive.material = material;
  mesh.primitives.push_back(primitive);
  return mesh;
}

SceneDesc textured() {
  SceneDesc scene = objects();
  scene.images = {{{1, 2, 3}, ".png"}, {{4, 5, 6}, ".png"}};
  MaterialDesc gold, silver;
  gold.name = "Gold";
  gold.base_color_image = 0;
  silver.name = "Silver";
  silver.base_color_image = 1;
  silver.roughness = 0.5f;
  scene.materials = {gold, silver};
  scene.meshes = {triangle("A", 0), triangle("B", 1)};
  scene.nodes[0].mesh = 0;
  scene.nodes[1].mesh = 1;
  return scene;
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("scene diff matches identities instead of node array positions") {
  const SceneDesc before = textured();
  SceneDesc after = before;
  std::swap(after.nodes[0], after.nodes[1]);
  CHECK(has(scene_difference(before, after, "main"), "+0 -0 ~0 objects\nno object changes"));
}

TEST_CASE("scene diff reports changed names, placement and properties under one identity") {
  SceneDesc before = objects();
  before.nodes[0].props["health"] = 10.0;
  SceneDesc after = before;
  after.nodes[0].name = "Renamed";
  after.nodes[0].local.position = {1, 2, 3};
  after.nodes[0].props["health"] = 9.0;
  const auto diff = scene_difference(before, after, "main");
  CHECK(has(diff, "+0 -0 ~1 objects"));
  CHECK(has(diff, "~ #one Renamed"));
  CHECK(has(diff, "name: \"One\" -> \"Renamed\""));
  CHECK(has(diff, "position: [0,0,0] -> [1,2,3]"));
  CHECK(has(diff, "props.health: 10.0 -> 9.0"));
}

TEST_CASE("scene diff does not rename survivors when another object is removed") {
  const SceneDesc before = objects();
  SceneDesc after = before;
  after.nodes.erase(after.nodes.begin());
  after.nodes.push_back(object("three", "Three"));
  const auto diff = scene_difference(before, after, "main");
  CHECK(has(diff, "+1 -1 ~0 objects"));
  CHECK(has(diff, "- #one One"));
  CHECK(has(diff, "+ #three Three"));
  CHECK_FALSE(has(diff, "~ #two"));
}

TEST_CASE("scene diff reports hierarchy through parent and child identities") {
  SceneDesc before = objects();
  before.nodes[0].children = {1};
  before.nodes[1].parent = 0;
  SceneDesc after = before;
  after.nodes[0].children.clear();
  after.nodes[1].parent = kNoIndex;
  const auto diff = scene_difference(before, after, "main");
  CHECK(has(diff, "parent: \"one\" -> null"));
  CHECK(has(diff, "children[0]: \"two\" -> <absent>"));
}

TEST_CASE("scene diff ignores remapped mesh, material and image array positions") {
  const SceneDesc before = textured();
  SceneDesc after = before;
  std::swap(after.images[0], after.images[1]);
  for (auto& material : after.materials) material.base_color_image = 1 - material.base_color_image;
  std::swap(after.materials[0], after.materials[1]);
  for (auto& mesh : after.meshes) mesh.primitives[0].material = 1 - mesh.primitives[0].material;
  std::swap(after.meshes[0], after.meshes[1]);
  for (auto& node : after.nodes) node.mesh = 1 - node.mesh;
  CHECK(has(scene_difference(before, after, "main"), "no object changes"));
}

TEST_CASE("scene diff reports sculpted geometry and changed picture bytes") {
  const SceneDesc before = textured();
  SceneDesc after = before;
  after.meshes[0].primitives[0].positions[0] = 0.25f;
  after.images[1].encoded.back() = 7;
  const auto diff = scene_difference(before, after, "main");
  CHECK(has(diff, "+0 -0 ~2 objects"));
  CHECK(has(diff, "geometry:"));
  CHECK(has(diff, "materials[0].images.color:"));
}

TEST_CASE("scene diff shows individual material, water and clearcoat parameters") {
  const SceneDesc before = textured();
  SceneDesc after = before;
  after.materials[0].roughness = 0.4f;
  after.materials[0].water.foam_width = 0.3f;
  after.materials[0].clearcoat.normal_scale = 0.5f;
  const auto diff = scene_difference(before, after, "main");
  CHECK(has(diff, "materials[0].roughness: 1 -> 0.4"));
  CHECK(has(diff, "materials[0].water.foam:"));
  CHECK(has(diff, "materials[0].clearcoat.normal_scale: 1 -> 0.5"));
}

TEST_CASE("scene diff reports camera and light settings") {
  SceneDesc before = objects();
  before.nodes[0].camera = CameraDesc{};
  before.nodes[1].light = LightDesc{};
  SceneDesc after = before;
  after.nodes[0].camera->far_plane = 1000;
  after.nodes[1].light->intensity = 10;
  const auto diff = scene_difference(before, after, "main");
  CHECK(has(diff, "camera.far: 0 -> 1000"));
  CHECK(has(diff, "light.intensity: 0 -> 10"));
}

TEST_CASE("scene diff reports animation keyframes and skin binding changes") {
  SceneDesc before = objects();
  SkinDesc skin;
  skin.joints = {1};
  skin.inverse_bind.resize(1);
  before.skins = {skin};
  before.nodes[0].skin = 0;
  AnimationChannel channel;
  channel.node = 1;
  channel.times = {0, 1};
  channel.values = {0, 0, 0, 1, 0, 0};
  before.animations = {{"Walk", 1, {channel}}};
  SceneDesc after = before;
  after.skins[0].inverse_bind[0][0] = 1;
  after.animations[0].channels[0].values.back() = 2;
  const auto diff = scene_difference(before, after, "main");
  CHECK(has(diff, "skin.bind:"));
  CHECK(has(diff, "animations[0].keys:"));
}

TEST_CASE("scene diff ignores quaternion sign and signed zero without masking tiny property edits") {
  SceneDesc before = textured();
  before.nodes[0].props["tiny"] = 0.0;
  SceneDesc after = before;
  after.nodes[0].local.rotation = {0, 0, 0, -1};
  after.meshes[0].primitives[0].positions[0] = -0.0f;
  CHECK(has(scene_difference(before, after, "main"), "no object changes"));
  after.nodes[0].props["tiny"] = 0.0000001;
  CHECK(has(scene_difference(before, after, "main"), "props.tiny:"));
}

TEST_CASE("scene diff keeps meter changes visible far from the origin") {
  SceneDesc before = objects();
  before.nodes[0].local.position.x = 1000000;
  SceneDesc after = before;
  after.nodes[0].local.position.x += 1;
  CHECK(has(scene_difference(before, after, "main"), "position:"));
}

TEST_CASE("scene diff limits detail without changing its complete counts") {
  const SceneDesc before = objects();
  SceneDesc after = before;
  after.nodes[0].name = "Changed";
  after.nodes[0].props["new"] = true;
  const auto short_diff = scene_difference(before, after, "main", 1);
  const auto full_diff = scene_difference(before, after, "main", 0);
  CHECK(has(short_diff, "+0 -0 ~1 objects"));
  CHECK(has(short_diff, "detail lines not shown"));
  CHECK_FALSE(has(full_diff, "detail lines not shown"));
  CHECK(has(full_diff, "props.new: <absent> -> true"));
}

TEST_CASE("scene diff bounds long text and cannot inject terminal controls") {
  const SceneDesc before = objects();
  SceneDesc after = before;
  after.nodes[0].name = "line\n\x1b[31m" + std::string(10000, 'x');
  after.nodes[0].props["message"] = std::string(10000, 'y');
  const auto diff = scene_difference(before, after, "main");
  CHECK(diff.size() < 2000);
  CHECK(diff.find('\x1b') == std::string::npos);
  CHECK(has(diff, "..."));
}

TEST_CASE("scene diff rejects duplicate identities rather than silently dropping an object") {
  SceneDesc scene = objects();
  scene.nodes[1].stable_id = scene.nodes[0].stable_id;
  CHECK_THROWS_AS(scene_difference(scene, scene, "main"), std::invalid_argument);
}
