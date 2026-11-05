#include <doctest/doctest.h>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>
#include "app/scene_files.hpp"
#include "editor/document/glb_file.hpp"
#include "scene/prefabs.hpp"
#include "editor/editor_session.hpp"
#include "editor/picking.hpp"
#include "test_support.hpp"
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
void cube_prefab(const PrefabFiles& files) {
  auto doc = SceneDocument::create_empty(files.dir / "prefabs/cube.glb");
  const int root = doc.add_primitive(PrimitiveShape::Cube, "Source", Color{1, 1, 1, 1});
  const int child = doc.add_primitive(PrimitiveShape::Cube, "Part", Color{1, 1, 1, 1}, root);
  Transform local;
  local.position = {2, 0, 0};
  doc.set_local(root, local, false);
  local.position = {0, 2, 0};
  doc.set_local(child, local, false);
  doc.set_prop(root, props::kPrefab, true, false);
  std::string error;
  REQUIRE_MESSAGE(doc.save(error), error);
}
std::unique_ptr<EditorSession> prefab_session(const PrefabFiles& files) {
  auto opened = EditorSession::open(files.dir);
  if (const auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<EditorSession>>(opened));
}
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
TEST_CASE("prefab origin metadata caches nested defaults and exact ordered root offsets") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  files.write("prefabs/outer.glb", nlohmann::json::parse(R"([
    {"name":"Outer", "translation":[0,0,4],
     "extras":{"prefab_file":"prefabs/item.glb","health":30,"tag":"source"}}
  ])"));
  auto authored = placements("prefabs/outer.glb");
  authored.nodes[0].local.position.x = 5;
  authored.nodes[0].props["health"] = 40.0;
  prefabs::Origins origins;
  const auto resolved = prefabs::resolve(authored, files.dir, {}, &origins);
  REQUIRE(origins.size() == 1);
  const auto& origin = origins.at(0);
  CHECK(origin.reference == "prefabs/outer.glb");
  CHECK(origin.name == "Outer");
  CHECK(origin.roots.size() == 2);
  CHECK(origin.defaults.at("health") == PropertyValue(30.0));
  CHECK(origin.defaults.count(props::kPrefab) == 0);
  CHECK(origin.defaults.count(prefabs::kFileProperty) == 0);
  CHECK(prefabs::inherited_props(origin, authored.nodes[0].props) == resolved.nodes[0].props);
  CHECK(prefabs::inherited_local(origin, authored.nodes[0].local).position.x == doctest::Approx(7));
  authored.nodes[0].props.erase("health");
  CHECK(prefabs::inherited_props(origin, authored.nodes[0].props).at("health") == PropertyValue(30.0));
}
TEST_CASE("failed prefab expansion leaves previously collected origins untouched") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  prefabs::Origins origins;
  prefabs::resolve(placements("prefabs/item.glb"), files.dir, {}, &origins);
  CHECK_THROWS(prefabs::resolve(placements("prefabs/missing.glb"), files.dir, {}, &origins));
  CHECK(origins.at(0).reference == "prefabs/item.glb");
}
TEST_CASE("editor prefab preview preserves inherited parts while dragging without rereading files") {
  PrefabFiles files;
  cube_prefab(files);
  auto raw = placements("prefabs/cube.glb");
  PrefabPreview preview;
  preview.rebuild(raw, files.dir);
  REQUIRE(preview.resolved() != nullptr);
  REQUIRE(preview.resolved()->nodes.size() == 2);
  const auto generation = preview.generation();
  const auto mesh = preview.resolved()->nodes[0].mesh;
  REQUIRE(mesh != kNoIndex);
  std::filesystem::remove(files.dir / "prefabs/cube.glb");
  raw.nodes[0].local.position.x = 5;
  REQUIRE(preview.refresh(raw, 0));
  CHECK(preview.resolved()->nodes[0].local.position.x == doctest::Approx(7));
  CHECK(preview.resolved()->nodes[0].mesh == mesh);
  CHECK(preview.resolved()->nodes[0].children == std::vector<int>{1});
  CHECK(preview.generation() == generation);
}
TEST_CASE("editor prefab preview selects inherited geometry through its authored reference and saves links") {
  PrefabFiles files;
  cube_prefab(files);
  files.write("scene.glb", nlohmann::json::parse(R"([
    {"name":"Placed", "extras":{"prefab_file":"prefabs/cube.glb"}}
  ])"));
  auto session = prefab_session(files);
  CHECK(session->scene_desc().nodes.size() == 1);
  REQUIRE(session->preview_desc().nodes.size() == 2);
  const PickRay ray{{2, 2, 5}, {0, 0, -1}};
  const auto hit = pick_object(session->scene(), session->preview_desc(), ray);
  REQUIRE(hit == 1);
  session->select(hit);
  CHECK(session->selection() == 0);
  CHECK(session->selected().ids() == std::vector<int>{0});
  Transform world;
  world.position = {5, 0, 0};
  session->document().checkpoint();
  session->place_in_world(0, world);
  CHECK(session->placement_world(0).position.x == doctest::Approx(5));
  CHECK(session->scene().world_transform(0).position.x == doctest::Approx(7));
  std::string error;
  REQUIRE_MESSAGE(session->save(error), error);
  const auto saved = load_gltf_scene((files.dir / "scene.glb").string());
  REQUIRE(saved.ok);
  REQUIRE(saved.scene.nodes.size() == 1);
  CHECK(saved.scene.meshes.empty());
  CHECK(get_string(saved.scene.nodes[0].props, prefabs::kFileProperty) == "prefabs/cube.glb");
  session->undo();
  CHECK(session->scene().world_transform(0).position.x == doctest::Approx(2));
  session->redo();
  CHECK(session->scene().world_transform(0).position.x == doctest::Approx(7));
  ScriptedInputSource input;
  REQUIRE_MESSAGE(session->play({input}, error), error);
  CHECK(session->scene().world_transform(0).position.x == doctest::Approx(7));
  session->stop();
  CHECK(session->scene().world_transform(0).position.x == doctest::Approx(7));
}
TEST_CASE("editor prefab failures leave references editable and coalesce repeated diagnostics") {
  PrefabFiles files;
  test::LogCapture logs;
  auto raw = placements("prefabs/missing.glb");
  PrefabPreview preview;
  preview.rebuild(raw, files.dir);
  preview.rebuild(raw, files.dir);
  CHECK(preview.resolved() == nullptr);
  CHECK(logs.count(log::Level::Error) == 1);
  cube_prefab(files);
  raw.nodes[0].props[prefabs::kFileProperty] = std::string("prefabs/cube.glb");
  preview.rebuild(raw, files.dir);
  REQUIRE(preview.resolved() != nullptr);
  CHECK(preview.resolved()->nodes.size() == 2);
}
TEST_CASE("editor prefab assets synchronize base edits and detect changed import offsets") {
  PrefabFiles files;
  cube_prefab(files);
  auto raw = placements("prefabs/cube.glb");
  raw.meshes.resize(1);
  raw.materials.resize(1);
  PrefabPreview preview;
  preview.rebuild(raw, files.dir);
  raw.materials[0].roughness = 0.25f;
  raw.meshes[0].name = "Edited terrain";
  REQUIRE(preview.resolved() != nullptr);
  REQUIRE(preview.sync_assets(raw, {0}));
  CHECK(preview.resolved()->materials[0].roughness == doctest::Approx(0.25f));
  CHECK(preview.resolved()->meshes[0].name == "Edited terrain");
  raw.images.emplace_back();
  CHECK_FALSE(preview.sync_assets(raw, {}));
}
TEST_CASE("editor copying and reparenting children of prefab references preserves their world placement") {
  PrefabFiles files;
  cube_prefab(files);
  files.write("scene.glb", nlohmann::json::parse(R"([
    {"name":"Placed", "children":[1], "extras":{"prefab_file":"prefabs/cube.glb"}},
    {"name":"Authored child", "translation":[0,3,0]}
  ])"));
  auto session = prefab_session(files);
  const int child = session->scene_desc().find_node("Authored child");
  REQUIRE(child != kNoIndex);
  CHECK(session->scene().world_transform(child).position.x == doctest::Approx(2));
  session->select(child);
  std::string error;
  const auto copied = session->copy_selection(error);
  REQUIRE_MESSAGE(copied.has_value(), error);
  REQUIRE_MESSAGE(session->paste(*copied, kNoObject, error), error);
  CHECK(session->scene().world_transform(session->selection()).position.x == doctest::Approx(2));
  session->undo();
  session->select_by_name("Authored child");
  session->reparent_selection(kNoObject);
  CHECK(session->scene().world_transform(session->selection()).position.x == doctest::Approx(2));
  session->undo();
  session->select_by_name("Authored child");
  const int placed = session->scene_desc().find_node("Placed");
  REQUIRE_MESSAGE(session->paste(*copied, placed, error), error);
  CHECK(session->scene().world_transform(session->selection()).position.x == doctest::Approx(2));
  CHECK(session->document().node(session->selection()).local.position.x == doctest::Approx(0));
}
TEST_CASE("editor prefab dragging refuses a composed transform overflow") {
  PrefabFiles files;
  files.write("prefabs/item.glb", kHierarchy);
  auto raw = placements("prefabs/item.glb");
  PrefabPreview preview;
  preview.rebuild(raw, files.dir);
  raw.nodes[0].local.scale.x = std::numeric_limits<float>::max();
  CHECK_FALSE(preview.refresh(raw, 0));
}
TEST_CASE("editor prefab copy budgets are checked before copying the authored scene") {
  PrefabFiles files;
  cube_prefab(files);
  const auto raw = placements("prefabs/cube.glb");
  prefabs::Limits limits;
  limits.expanded_bytes = 1;
  test::LogCapture logs;
  PrefabPreview preview;
  preview.rebuild(raw, files.dir, limits);
  CHECK(preview.resolved() == nullptr);
  CHECK(logs.contains("prefab preview copy budget"));
}
TEST_CASE("editor node refresh detects newly added prefab links and edits to broken links") {
  PrefabFiles files;
  cube_prefab(files);
  SceneDesc raw;
  raw.nodes.emplace_back();
  PrefabPreview preview;
  preview.rebuild(raw, files.dir);
  raw.nodes[0].props[prefabs::kFileProperty] = std::string("prefabs/missing.glb");
  CHECK_FALSE(preview.refresh(raw, 0));
  preview.rebuild(raw, files.dir);
  const auto generation = preview.generation();
  raw.nodes[0].local.position.x = 4;
  CHECK(preview.refresh(raw, 0));
  CHECK(preview.generation() == generation);
  raw.nodes[0].props[prefabs::kFileProperty] = std::string("prefabs/cube.glb");
  CHECK_FALSE(preview.refresh(raw, 0));
  preview.rebuild(raw, files.dir);
  REQUIRE(preview.resolved() != nullptr);
  CHECK(preview.resolved()->nodes[0].local.position.x == doctest::Approx(6));
}

TEST_CASE("placing a prefab keeps a linked document through undo redo save reopen and Play") {
  PrefabFiles files;
  cube_prefab(files);
  files.write("scene.glb", nlohmann::json::array({{{"name", "Existing"}}}));
  auto session = prefab_session(files);
  std::string error;
  const Vec3 at{5, 4, 3};
  const int placed = session->place_prefab("prefabs/cube.glb", at, error);
  REQUIRE_MESSAGE(placed != kNoObject, error);
  CHECK(session->selection() == placed);
  CHECK(session->scene_desc().nodes.size() == 2);
  CHECK(session->scene_desc().meshes.empty());
  CHECK(session->document().node(placed).mesh == kNoIndex);
  CHECK(get_string(session->document().node(placed).props, props::kPrefabFile) == "prefabs/cube.glb");
  const auto id = session->document().node(placed).stable_id;
  CHECK(approx_equal(session->placement_world(placed).position, at, test::kTolerance));
  CHECK(approx_equal(session->scene().world_transform(placed).position, at + Vec3{2, 0, 0}, test::kTolerance));
  CHECK(session->preview_desc().meshes.size() == 2);
  session->undo();
  CHECK(session->scene_desc().nodes.size() == 1);
  CHECK_FALSE(session->document().dirty());
  session->redo();
  CHECK(session->document().node(placed).stable_id == id);
  REQUIRE_MESSAGE(session->save(error), error);
  session = prefab_session(files);
  CHECK(session->document().node(placed).stable_id == id);
  CHECK(session->scene_desc().meshes.empty());
  ScriptedInputSource input;
  REQUIRE_MESSAGE(session->play({input}, error), error);
  CHECK(session->scene().size() == 3);
  CHECK(session->place_prefab("prefabs/cube.glb", at, error) == kNoObject);
  CHECK_FALSE(error.empty());
  session->stop();
  CHECK(session->scene_desc().nodes.size() == 2);
}
TEST_CASE("prefab placement uses inherited parent offsets and distinct stable identities") {
  PrefabFiles files;
  cube_prefab(files);
  files.write("scene.glb", nlohmann::json::array({{{"name", "Existing"}}}));
  auto session = prefab_session(files);
  std::string error;
  const int parent = session->place_prefab("prefabs/cube.glb", {3, 0, 0}, error);
  REQUIRE(parent != kNoObject);
  Transform pose = session->document().node(parent).local;
  pose.rotation = quat_from_axis_angle(Vec3::up(), kPi / 3);
  pose.scale = {2, 3, 4};
  session->document().set_local(parent, pose);
  session->refresh_node(parent);
  const Vec3 at{12, 5, -4};
  const int child = session->place_prefab("prefabs/cube.glb", at, error, parent);
  REQUIRE_MESSAGE(child != kNoObject, error);
  CHECK(session->document().node(child).parent == parent);
  CHECK(approx_equal(session->placement_world(child).position, at, test::kTolerance));
  CHECK(session->document().node(child).stable_id != session->document().node(parent).stable_id);
  CHECK(session->document().node(child).name != session->document().node(parent).name);
  CHECK(session->preview_desc().meshes.size() == 2);
  CHECK(session->authored_object(static_cast<int>(session->preview_desc().nodes.size()) - 1) == child);
  pose.scale.y = 0;
  session->document().set_local(parent, pose);
  session->refresh_node(parent);
  const auto count = session->document().node_count();
  CHECK(session->place_prefab("prefabs/cube.glb", at, error, parent) == kNoObject);
  CHECK(error.find("zero scale") != std::string::npos);
  CHECK(session->document().node_count() == count);
  CHECK(session->selection() == child);
}
TEST_CASE("rejected prefab placements preserve document selection and redo history") {
  test::LogCapture quiet;
  PrefabFiles files;
  cube_prefab(files);
  files.write("scene.glb", nlohmann::json::array({{{"name", "Existing"}}}));
  files.write("prefabs/cycle.glb", nlohmann::json::array({{{"extras", {{"prefab_file", "prefabs/cycle.glb"}}}}}));
  auto session = prefab_session(files);
  std::string error;
  REQUIRE(session->place_prefab("prefabs/cube.glb", {}, error) != kNoObject);
  session->undo();
  session->select(0);
  for (const auto* file : {"prefabs/missing.glb", "../cube.glb", "prefabs/cycle.glb", ""}) {
    CAPTURE(file);
    CHECK(session->place_prefab(file, {}, error) == kNoObject);
    CHECK_FALSE(error.empty());
    CHECK(session->document().node_count() == 1);
    CHECK(session->selection() == 0);
    CHECK_FALSE(session->document().dirty());
    CHECK(session->document().can_redo());
  }
  CHECK(session->place_prefab("prefabs/cube.glb", {}, error, 500) == kNoObject);
  CHECK(session->place_prefab("prefabs/cube.glb", {std::numeric_limits<float>::infinity(), 0, 0}, error) == kNoObject);
  CHECK(session->document().can_redo());
  session->redo();
  CHECK(session->document().node_count() == 2);
}

TEST_CASE("nested prefab placement resolves dependencies without flattening or stale asset generations") {
  PrefabFiles files;
  cube_prefab(files);
  files.write("scene.glb", nlohmann::json::array({{{"name", "Existing"}}}));
  files.write("prefabs/nested.glb", nlohmann::json::array({{{"extras", {{"prefab_file", "prefabs/cube.glb"}}}}}));
  auto session = prefab_session(files);
  std::string error;
  for (int i = 0; i < 3; ++i) {
    const auto before = session->preview_generation();
    REQUIRE_MESSAGE(session->place_prefab("prefabs/nested.glb", {static_cast<float>(i), 0, 0}, error) != kNoObject,
                    error);
    CHECK(session->preview_generation() > before);
    CHECK(session->scene_desc().meshes.empty());
    CHECK(session->preview_desc().meshes.size() == 2);
    CHECK(session->scene().size() == 1 + (i + 1) * 2);
  }
}
