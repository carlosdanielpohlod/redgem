#include <doctest/doctest.h>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>
#include "app/scene_files.hpp"
#include "editor/document/glb_file.hpp"
#include "scene/prefabs.hpp"
#include "scene/prefab_import.hpp"
#include "scene/scene.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/animation/animation_player.hpp"
#include "scene/scene_ids.hpp"
#include "scene/props.hpp"

using namespace redgem;
namespace {
struct PrefabFiles {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem-prefabs-" + make_scene_id());
  PrefabFiles() { std::filesystem::create_directories(dir / "prefabs"); }
  ~PrefabFiles() {
    std::error_code error;
    std::filesystem::remove_all(dir, error);
  }
  void write(const std::string& name, const nlohmann::json& nodes) const {
    GlbFile file;
    file.json = {{"asset", {{"version", "2.0"}}}, {"scene", 0}, {"scenes", {{{"nodes", {0}}}}}, {"nodes", nodes}};
    std::string error;
    REQUIRE_MESSAGE(write_glb(dir / name, file, error), error);
  }
};
SceneDesc placements(const std::string& file, std::size_t count = 1) {
  SceneDesc scene;
  for (std::size_t i = 0; i < count; ++i) {
    NodeDesc node;
    node.name = "Placed" + std::to_string(i);
    node.stable_id = "placement-" + std::to_string(i);
    node.props[prefabs::kFileProperty] = file;
    scene.nodes.push_back(node);
  }
  return scene;
}
const nlohmann::json kHierarchy = nlohmann::json::parse(R"([
  {"name":"Root", "translation":[2,0,0], "children":[1], "extras":{"prefab":true,"health":10}},
  {"name":"Child", "translation":[0,3,0]}
])");
}  // namespace
TEST_CASE("prefab roots retain placement identity and overrides while descendants get unique persistent ids") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  auto input = placements("prefabs/item.glb", 2);
  input.nodes[0].local.position.x = 5;
  input.nodes[0].props["health"] = 20.0;
  const auto scene = prefabs::resolve(input, files.dir);
  REQUIRE(scene.nodes.size() == 4);
  CHECK(scene.nodes[0].name == "Placed0");
  CHECK(scene.nodes[0].stable_id == "placement-0");
  CHECK(scene.nodes[0].local.position.x == doctest::Approx(7));
  CHECK(scene.nodes[0].props.count(props::kPrefab) == 0);
  CHECK(scene.nodes[0].props.count(prefabs::kFileProperty) == 0);
  CHECK(scene.nodes[0].props.at("health") == input.nodes[0].props.at("health"));
  CHECK(scene.nodes[2].parent == 0);
  CHECK(scene.nodes[3].parent == 1);
  CHECK(scene.nodes[2].stable_id != scene.nodes[3].stable_id);
  CHECK(valid_scene_id(scene.nodes[2].stable_id));
  CHECK(prefabs::resolve(input, files.dir).nodes[2].stable_id == scene.nodes[2].stable_id);
}
TEST_CASE("prefab resolution follows nested roots and rejects cycles without mutating input") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  files.write("prefabs/outer.glb", nlohmann::json::parse(R"([{"extras":{"prefab_file":"prefabs/item.glb"}}])"));
  auto input = placements("prefabs/outer.glb");
  CHECK(prefabs::resolve(input, files.dir).nodes.size() == 2);
  files.write("prefabs/item.glb", nlohmann::json::parse(R"([{"extras":{"prefab_file":"prefabs/outer.glb"}}])"));
  CHECK_THROWS_WITH(prefabs::resolve(input, files.dir), "prefab reference cycle");
  CHECK(input.nodes.size() == 1);
  CHECK(input.nodes[0].props.count(prefabs::kFileProperty) == 1);
}
TEST_CASE("prefab expansion enforces budgets and game-relative paths") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  const auto input = placements("prefabs/item.glb", 2);
  prefabs::Limits limits;
  SUBCASE("nodes") { limits.nodes = 3; }
  SUBCASE("instances") { limits.instances = 1; }
  SUBCASE("input") { limits.input_bytes = 1; }
  SUBCASE("expanded") { limits.expanded_bytes = 1; }
  SUBCASE("files") { limits.files = 0; }
  SUBCASE("nesting") { limits.depth = 0; }
  CHECK_THROWS(prefabs::resolve(input, files.dir, limits));
  CHECK_THROWS(prefabs::resolve(placements("../outside.glb"), files.dir));
  CHECK_THROWS(prefabs::resolve(placements("prefabs/missing.glb"), files.dir));
}
TEST_CASE("scene file loading resolves prefabs and reports broken references as errors") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  files.write("scene.glb", nlohmann::json::parse(R"([{"extras":{"prefab_file":"prefabs/item.glb"}}])"));
  const auto loaded = read_game_scene(files.dir);
  REQUIRE(std::holds_alternative<SceneDesc>(loaded));
  CHECK(std::get<SceneDesc>(loaded).nodes.size() == 2);
  std::filesystem::remove(files.dir / "prefabs/item.glb");
  CHECK(std::holds_alternative<std::string>(read_game_scene(files.dir)));
}
TEST_CASE("scenes without prefab references require no existing game folder") {
  SceneDesc scene;
  scene.nodes.emplace_back();
  CHECK(prefabs::resolve(scene, "/missing-redgem-game").nodes.size() == 1);
}
TEST_CASE("prefab references reject nonempty placements and malformed properties") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  auto input = placements("prefabs/item.glb");
  SUBCASE("geometry") { input.nodes[0].mesh = 0; }
  SUBCASE("light") { input.nodes[0].light = LightDesc{}; }
  SUBCASE("wrong type") { input.nodes[0].props[prefabs::kFileProperty] = true; }
  CHECK_THROWS(prefabs::resolve(input, files.dir));
}
TEST_CASE("animated placements cannot silently discard a source root transform") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  auto input = placements("prefabs/item.glb");
  AnimationDesc animation;
  AnimationChannel channel;
  channel.node = 0;
  channel.times = {0, 1};
  channel.values = {0, 0, 0, 1, 0, 0};
  animation.channels.push_back(channel);
  input.animations.push_back(animation);
  CHECK_THROWS_WITH(prefabs::resolve(input, files.dir),
                    "animated prefab placements require an identity source root transform");
}
TEST_CASE("prefab expansion rejects composed hierarchies beyond the loader depth budget") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  auto input = placements("prefabs/item.glb");
  constexpr std::size_t kDepthLimit = 256;
  input.nodes.resize(kDepthLimit);
  for (std::size_t i = 0; i + 1 < input.nodes.size(); ++i) {
    input.nodes[i].props.clear();
    input.nodes[i].children = {static_cast<int>(i + 1)};
    input.nodes[i + 1].parent = static_cast<int>(i);
  }
  input.nodes.back().props[prefabs::kFileProperty] = std::string("prefabs/item.glb");
  CHECK_THROWS_WITH(prefabs::resolve(input, files.dir), "expanded prefab hierarchy is too deep");
}
TEST_CASE("prefab instances share geometry but remap skeletons and animation targets independently") {
  prefabs::Source source;
  source.scene.nodes.resize(2);
  source.scene.nodes[0].children = {1};
  source.scene.nodes[0].mesh = 0;
  source.scene.nodes[0].skin = 0;
  source.scene.nodes[1].parent = 0;
  source.scene.nodes[1].stable_id = "joint";
  SkinDesc skin;
  skin.joints = {0, 1};
  source.scene.skins.push_back(skin);
  AnimationDesc animation;
  animation.name = "Idle";
  animation.duration = 1;
  AnimationChannel channel;
  channel.node = 1;
  channel.times = {0, 1};
  channel.values = {0, 0, 0, 2, 0, 0};
  animation.channels.push_back(channel);
  source.scene.animations.push_back(animation);
  source.scene.images.resize(1);
  source.scene.materials.resize(1);
  source.scene.materials[0].base_color_image = 0;
  source.scene.materials[0].normal_image = 0;
  source.scene.materials[0].clearcoat.normal_image = 0;
  MeshDesc mesh;
  PrimitiveDesc primitive;
  primitive.material = 0;
  mesh.primitives.push_back(primitive);
  source.scene.meshes.push_back(mesh);
  auto target = placements("unused", 2);
  target.images.resize(2);
  target.materials.resize(3);
  target.meshes.resize(4);
  prefabs::transfer_assets(target, source);
  prefabs::append_instance(target, source, 0, "prefabs/item.glb");
  prefabs::transfer_assets(target, source);
  prefabs::append_instance(target, source, 1, "prefabs/item.glb");
  REQUIRE(target.nodes.size() == 4);
  REQUIRE(target.meshes.size() == 5);
  REQUIRE(target.materials.size() == 4);
  REQUIRE(target.images.size() == 3);
  CHECK(target.nodes[0].mesh == 4);
  CHECK(target.nodes[1].mesh == 4);
  CHECK(target.meshes[4].primitives[0].material == 3);
  CHECK(target.materials[3].base_color_image == 2);
  CHECK(target.materials[3].normal_image == 2);
  CHECK(target.materials[3].clearcoat.normal_image == 2);
  CHECK(target.materials[3].emissive_image == kNoIndex);
  CHECK(target.nodes[0].skin == 0);
  CHECK(target.nodes[1].skin == 1);
  CHECK(target.skins[0].joints == std::vector<int>{0, 2});
  CHECK(target.skins[1].joints == std::vector<int>{1, 3});
  CHECK(target.animations[0].channels[0].node == 2);
  REQUIRE(target.animations.size() == 1);
  REQUIRE(target.animations[0].channels.size() == 2);
  CHECK(target.animations[0].channels[1].node == 3);
  CHECK(target.animations[0].name == "prefabs/item.glb::Idle");
  auto world = Scene::from_desc(target);
  AnimationPlayer player;
  player.reset(target.animations, world);
  REQUIRE(player.stop("prefabs/item.glb::Idle"));
  REQUIRE(player.play_on(world, 1, "prefabs/item.glb::Idle", {}));
  player.advance(0.5f, world);
  CHECK(world.at(2).local.position.x == doctest::Approx(0));
  CHECK(world.at(3).local.position.x == doctest::Approx(1));
}
TEST_CASE("inert prefab cameras never become the active camera") {
  SceneDesc description;
  NodeDesc camera;
  camera.camera = CameraDesc{};
  camera.props[props::kPrefab] = true;
  description.nodes.push_back(camera);
  camera.props.clear();
  description.nodes.push_back(camera);
  auto scene = Scene::from_desc(description);
  CHECK(scene.main_camera() == 1);
}
TEST_CASE("tighter glTF byte budgets reject files before constructing their scene") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  const auto loaded = load_gltf_scene((files.dir / "prefabs/item.glb").string(), 1);
  CHECK_FALSE(loaded.ok);
  CHECK(loaded.error.find("loading byte budget") != std::string::npos);
  CHECK(loaded.scene.nodes.empty());
}

TEST_CASE("prefab placement transform overflow is rejected before world creation") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  auto scene = placements("prefabs/item.glb");
  scene.nodes[0].local.scale.x = std::numeric_limits<float>::max();
  CHECK_THROWS_WITH(prefabs::resolve(scene, files.dir), "prefab placement transform is not finite");
}
TEST_CASE("prefab dependencies cannot be reached through symbolic links") {
#ifndef _WIN32
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  std::filesystem::create_symlink(files.dir / "prefabs/item.glb", files.dir / "prefabs/linked.glb");
  CHECK_THROWS(prefabs::resolve(placements("prefabs/linked.glb"), files.dir));
#endif
}
TEST_CASE("prefab child identities cannot alias an authored placement identity") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  auto input = placements("prefabs/item.glb", 2);
  const auto first = prefabs::resolve(input, files.dir);
  input.nodes[1].stable_id = first.nodes[2].stable_id;
  CHECK_THROWS_WITH(prefabs::resolve(input, files.dir), "prefab identity collision");
}
TEST_CASE("prefab identities normalize harmless path aliases but reject parent traversal") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  const auto direct = prefabs::resolve(placements("prefabs/item.glb"), files.dir);
  const auto alias = prefabs::resolve(placements("prefabs/./item.glb"), files.dir);
  CHECK(direct.nodes[1].stable_id == alias.nodes[1].stable_id);
  CHECK_THROWS(prefabs::resolve(placements("prefabs/../prefabs/item.glb"), files.dir));
}
