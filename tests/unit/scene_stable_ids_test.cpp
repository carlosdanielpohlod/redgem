#include <doctest/doctest.h>

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <variant>

#include "editor/document/scene_document.hpp"
#include "project/scene_lookup.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/scene_ids.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace fs = std::filesystem;

namespace {

struct TempScene {
  fs::path path;
  explicit TempScene(const std::string& name) : path(fs::temp_directory_path() / ("redgem_ids_" + name + ".glb")) {}
  ~TempScene() { fs::remove(path); }
};

SceneDocument opened(const fs::path& path) {
  auto loaded = SceneDocument::open(path);
  if (const auto* problem = std::get_if<std::string>(&loaded)) FAIL(*problem);
  return std::move(std::get<SceneDocument>(loaded));
}

void save(SceneDocument& document) {
  std::string error;
  REQUIRE_MESSAGE(document.save(error), error);
}

std::map<std::string, std::string> identities(const SceneDesc& scene) {
  std::map<std::string, std::string> ids;
  for (const NodeDesc& node : scene.nodes) ids[node.name] = node.stable_id;
  return ids;
}

bool unique_identities(const SceneDesc& scene) {
  std::set<std::string> ids;
  for (const NodeDesc& node : scene.nodes) {
    if (!valid_scene_id(node.stable_id) || !ids.insert(node.stable_id).second) return false;
  }
  return true;
}

}  // namespace

TEST_CASE("an object's identity survives rename, reparent, deletion before it and reopening") {
  const TempScene file("edits");
  SceneDocument document = SceneDocument::create_empty(file.path);
  const int target = document.add_empty("Target");
  document.add_empty("Erase");
  const int holder = document.add_empty("Holder");
  const std::string id = document.node(target).stable_id;
  const std::string holder_id = document.node(holder).stable_id;
  document.set_name(target, "Renamed");
  document.reparent(target, holder);
  document.remove(document.desc().find_node("Erase"));
  save(document);
  const SceneDocument read = opened(file.path);
  const auto found = tools::find_object(read.desc(), "#" + id);
  REQUIRE(found.index != kNoIndex);
  CHECK(read.node(found.index).name == "Renamed");
  CHECK(read.node(found.index).stable_id == id);
  CHECK(found.index != target);
  CHECK(read.node(read.node(found.index).parent).stable_id == holder_id);
}

TEST_CASE("duplicated subtrees get independent identities and keep them on save") {
  const TempScene file("duplicates");
  SceneDocument document = SceneDocument::create_empty(file.path);
  const int root = document.add_empty("Root");
  document.add_empty("Child", root);
  const auto originals = identities(document.desc());
  const int copy = document.duplicate(root);
  CHECK(document.node(copy).stable_id != originals.at("Root"));
  CHECK(unique_identities(document.desc()));
  const auto before = identities(document.desc());
  save(document);
  CHECK(identities(opened(file.path).desc()) == before);
}

TEST_CASE("pasting into either the source scene or another scene creates new identities") {
  const TempScene file("paste_source");
  const TempScene other_file("paste_target");
  SceneDocument source = SceneDocument::create_empty(file.path);
  const int root = source.add_empty("Root");
  source.add_empty("Child", root);
  const auto original_ids = identities(source.desc());
  std::string error;
  const auto bytes = source.copy_nodes({root}, error);
  REQUIRE_MESSAGE(bytes, error);
  const auto pasted = source.paste_nodes(*bytes, kNoIndex, error);
  REQUIRE_MESSAGE(!pasted.empty(), error);
  CHECK(source.node(pasted.front()).stable_id != original_ids.at("Root"));
  CHECK(unique_identities(source.desc()));
  SceneDocument target = SceneDocument::create_empty(other_file.path);
  const auto cross_scene = target.paste_nodes(*bytes, kNoIndex, error);
  REQUIRE_MESSAGE(!cross_scene.empty(), error);
  for (const NodeDesc& node : target.desc().nodes) CHECK(node.stable_id != original_ids.at(node.name));
  const auto before = identities(target.desc());
  save(target);
  CHECK(identities(opened(other_file.path).desc()) == before);
}

TEST_CASE("imported models get identities of their destination rather than copying the source's") {
  const TempScene model_file("import_model");
  SceneDocument model = SceneDocument::create_empty(model_file.path);
  model.add_empty("ModelRoot");
  const auto model_ids = identities(model.desc());
  save(model);
  const TempScene target_file("import_target");
  SceneDocument target = SceneDocument::create_empty(target_file.path);
  target.add_empty("Existing");
  std::string error;
  REQUIRE_MESSAGE(target.import_model(model_file.path, error) != kNoIndex, error);
  const int imported = target.desc().find_node("ModelRoot");
  REQUIRE(imported != kNoIndex);
  CHECK(target.node(imported).stable_id != model_ids.at("ModelRoot"));
  CHECK(unique_identities(target.desc()));
  const auto before = identities(target.desc());
  save(target);
  CHECK(identities(opened(target_file.path).desc()) == before);
}

TEST_CASE("undo and redo restore identities while another new object gets its own") {
  const TempScene file("history");
  SceneDocument document = SceneDocument::create_empty(file.path);
  document.add_empty("Root");
  const int made = document.add_empty("Made");
  const std::string id = document.node(made).stable_id;
  REQUIRE(document.undo());
  CHECK(document.desc().find_node("Made") == kNoIndex);
  REQUIRE(document.redo());
  CHECK(document.node(document.desc().find_node("Made")).stable_id == id);
  REQUIRE(document.undo());
  const int another = document.add_empty("Another");
  CHECK(document.node(another).stable_id != id);
}

TEST_CASE("legacy files keep their initial numeric identities when first saved") {
  SceneDocument document = opened(test::fixture("hierarchy.glb"));
  const auto before = identities(document.desc());
  const TempScene file("legacy");
  std::string error;
  REQUIRE_MESSAGE(document.save_as(file.path, error), error);
  const SceneDocument after = opened(file.path);
  CHECK(identities(after.desc()) == before);
  CHECK(unique_identities(after.desc()));
}

TEST_CASE("invalid and duplicate identities are repaired with a warning and valid ones are kept") {
  SceneDesc scene;
  scene.nodes.resize(4);
  for (std::size_t i = 0; i < scene.nodes.size(); ++i) scene.nodes[i].name = std::to_string(i);
  scene.nodes[0].stable_id = "kept";
  scene.nodes[1].stable_id = "kept";
  scene.nodes[2].stable_id = "bad id";
  const auto warnings = assign_scene_ids(scene);
  CHECK(warnings.size() == 2);
  CHECK(scene.nodes[0].stable_id == "kept");
  CHECK(unique_identities(scene));
  const auto first = identities(scene);
  CHECK(assign_scene_ids(scene).empty());
  CHECK(identities(scene) == first);
}
