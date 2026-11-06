#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include "core/config.hpp"
#include "core/files.hpp"
#include "editor/editor_session.hpp"
#include "editor/node_tree.hpp"
#include "editor/prefab_files.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/prefabs.hpp"
#include "scene/scene_ids.hpp"
#include "scene/animation/animation_player.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace {
struct ExportGame {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem-export-" + make_scene_id());
  ExportGame() { std::filesystem::create_directories(dir); }
  ~ExportGame() {
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
  }
  std::filesystem::path file(const std::string& name) const { return dir / "prefabs" / (name + ".glb"); }
};
SceneDocument document_at(const std::filesystem::path& file) {
  auto opened = SceneDocument::open(file);
  if (const auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<SceneDocument>(opened));
}
std::unique_ptr<EditorSession> session_at(const ExportGame& game) {
  auto opened = EditorSession::open(game.dir);
  if (const auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<EditorSession>>(opened));
}
SceneDesc load(const std::filesystem::path& file) {
  auto result = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(result.ok, result.error);
  return std::move(result.scene);
}
SceneDesc place(const ExportGame& game, const std::string& name, Vec3 position = {}) {
  SceneDesc scene;
  NodeDesc node;
  node.local.position = position;
  node.props[props::kPrefabFile] = "prefabs/" + name + ".glb";
  scene.nodes.push_back(node);
  return prefabs::resolve(std::move(scene), game.dir);
}
void check_pose(const Transform& actual, const Transform& expected) {
  CHECK(approx_equal(actual.position, expected.position, test::kTolerance));
  CHECK(approx_equal(actual.rotation, expected.rotation, test::kTolerance));
  CHECK(approx_equal(actual.scale, expected.scale, test::kTolerance));
}
}  // namespace

TEST_CASE("prefab export retains a selected hierarchy at its pivot without changing its scene") {
  ExportGame game;
  auto document = SceneDocument::create_empty(game.dir / config::kSceneFileName);
  const int parent = document.add_empty("Parent");
  const int root = document.add_primitive(PrimitiveShape::Cube, "Crate", Color{1, 0, 0, 1}, parent);
  const int child = document.add_empty("Part", root);
  document.add_primitive(PrimitiveShape::Sphere, "Unrelated", Color{0, 0, 1, 1});
  Transform pose;
  pose.position = {5, 2, 3};
  pose.scale = {2, 3, 4};
  pose.rotation = quat_from_axis_angle(Vec3::up(), kPi / 3);
  document.set_local(parent, pose);
  pose.position = {1, 1, 1};
  document.set_local(root, pose);
  std::string error;
  REQUIRE(document.save(error));
  auto session = session_at(game);
  session->select(root);
  const auto before = session->placement_world(root);
  REQUIRE_MESSAGE(session->export_prefab("crate", error), error);
  CHECK_FALSE(session->document().dirty());
  CHECK_FALSE(session->document().can_undo());
  CHECK(session->selection() == root);
  check_pose(session->placement_world(root), before);
  const auto source = load(game.file("crate"));
  REQUIRE(source.nodes.size() == 2);
  CHECK(source.meshes.size() == 1);
  CHECK(source.nodes[0].stable_id == document.node(root).stable_id);
  CHECK(source.nodes[1].stable_id == document.node(child).stable_id);
  Transform expected = before;
  expected.position = {};
  check_pose(source.nodes[0].local, expected);
  const auto placed = place(game, "crate", before.position);
  check_pose(placed.nodes[0].local, before);
  REQUIRE(session->place_prefab("prefabs/crate.glb", {}, error) != kNoObject);
  session->undo();
  CHECK_FALSE(session->document().dirty());
}
TEST_CASE("prefab export refuses invalid names existing files and linked destinations") {
  ExportGame game;
  auto document = SceneDocument::create_empty(game.dir / config::kSceneFileName);
  const int root = document.add_empty("Root");
  std::string error;
  for (const auto* name : {"", "../other", "Upper", "a/b", "a.glb", "a:b", "a\\b"}) {
    CAPTURE(name);
    CHECK_FALSE(prefab_files::save(document, root, {}, game.dir, name, error));
    CHECK_FALSE(error.empty());
  }
  CHECK_FALSE(prefab_files::valid_name(std::string(65, 'a')));
  REQUIRE(prefab_files::save(document, root, {}, game.dir, "valid-name_2", error));
  const auto before = read_text_file(game.file("valid-name_2"), error);
  CHECK_FALSE(prefab_files::save(document, root, {}, game.dir, "valid-name_2", error));
  CHECK(read_text_file(game.file("valid-name_2"), error) == before);
  std::filesystem::create_symlink(game.file("valid-name_2"), game.file("linked"));
  CHECK_FALSE(prefab_files::save(document, root, {}, game.dir, "linked", error));
  std::filesystem::rename(game.dir / "prefabs", game.dir / "elsewhere");
  std::filesystem::create_directory_symlink(game.dir / "elsewhere", game.dir / "prefabs");
  CHECK_FALSE(prefab_files::save(document, root, {}, game.dir, "outside", error));
  CHECK_FALSE(std::filesystem::exists(game.dir / "elsewhere/outside.glb"));
}
TEST_CASE("prefab export validates dependencies and removes staging files on failure") {
  test::LogCapture quiet;
  ExportGame game;
  auto document = SceneDocument::create_empty(game.dir / config::kSceneFileName);
  const int root = document.add_empty("Root");
  document.set_prop(root, props::kPrefabFile, std::string("prefabs/missing.glb"));
  std::string error;
  CHECK_FALSE(prefab_files::save(document, root, {}, game.dir, "broken", error));
  CHECK_FALSE(std::filesystem::exists(game.file("broken")));
  for (const auto& entry : std::filesystem::directory_iterator(game.dir)) CHECK(entry.path().filename() == "prefabs");
  document.remove_prop(root, props::kPrefabFile);
  REQUIRE(prefab_files::save(document, root, {}, game.dir, "base", error));
  document.set_prop(root, props::kPrefabFile, std::string("prefabs/base.glb"));
  REQUIRE_MESSAGE(prefab_files::save(document, root, {}, game.dir, "nested", error), error);
  const auto nested = load(game.file("nested"));
  CHECK(get_string(nested.nodes[0].props, props::kPrefabFile) == "prefabs/base.glb");
  CHECK(place(game, "nested").nodes.size() == 1);
}
TEST_CASE("prefab export refuses incomplete skeletons and root animation while preserving a complete rig") {
  ExportGame game;
  auto document = document_at(test::fixture("skinned.glb"));
  std::string error;
  CHECK_FALSE(prefab_files::save(document, document.desc().find_node("Arm"), {}, game.dir, "incomplete", error));
  CHECK(error.find("joint") != std::string::npos);
  CHECK_FALSE(prefab_files::save(document, document.desc().find_node("Lower"), {}, game.dir, "animated-root", error));
  CHECK(error.find("descendants") != std::string::npos);
  const int rig = document.desc().find_node("Rig");
  Transform pose;
  pose.position = {5, 3, -4};
  pose.rotation = quat_from_axis_angle(Vec3::up(), kPi / 4);
  pose.scale = {2, 2, 2};
  document.set_local(rig, pose);
  REQUIRE_MESSAGE(prefab_files::save(document, rig, pose, game.dir, "rig", error), error);
  const auto exported = place(game, "rig", pose.position);
  REQUIRE(exported.skins.size() == 1);
  CHECK(exported.skins[0].joints.size() == document.desc().skins[0].joints.size());
  CHECK(exported.skins[0].inverse_bind == document.desc().skins[0].inverse_bind);
  Scene original = Scene::from_desc(document.desc());
  Scene copy = Scene::from_desc(exported);
  AnimationPlayer original_player, copy_player;
  original_player.reset(document.desc().animations, original);
  copy_player.reset(exported.animations, copy);
  for (int step = 0; step < 3; ++step) {
    original_player.advance(0.25f, original);
    copy_player.advance(0.25f, copy);
    for (const auto* name : {"Arm", "Lower", "Upper"})
      check_pose(copy.world_transform(copy.find(name)), original.world_transform(original.find(name)));
  }
}

TEST_CASE("prefab export needs one top-level selection and is disabled during Play") {
  ExportGame game;
  auto document = SceneDocument::create_empty(game.dir / config::kSceneFileName);
  const int root = document.add_empty("Root");
  const int child = document.add_empty("Child", root);
  const int other = document.add_empty("Other");
  std::string error;
  REQUIRE(document.save(error));
  auto session = session_at(game);
  CHECK_FALSE(session->can_export_prefab());
  CHECK_FALSE(session->export_prefab("none", error));
  session->select_all({root, other});
  CHECK_FALSE(session->can_export_prefab());
  CHECK_FALSE(session->export_prefab("several", error));
  session->select_all({root, child});
  CHECK(session->can_export_prefab());
  REQUIRE_MESSAGE(session->export_prefab("hierarchy", error), error);
  CHECK(load(game.file("hierarchy")).nodes.size() == 2);
  ScriptedInputSource input;
  REQUIRE_MESSAGE(session->play({input}, error), error);
  CHECK_FALSE(session->can_export_prefab());
  CHECK_FALSE(session->export_prefab("playing", error));
  CHECK_FALSE(std::filesystem::exists(game.file("playing")));
  session->stop();
}
