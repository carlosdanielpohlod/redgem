#include <doctest/doctest.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "app/scene_files.hpp"
#include "editor/document/glb_file.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/prefab_overrides.hpp"
#include "scene/prefabs.hpp"
#include "scene/props.hpp"
#include "scene/scene_ids.hpp"
#include "test_support.hpp"

using namespace redgem;
using prefabs::Overrides;
using prefabs::PartOverride;

namespace {
struct GameFolder {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem-overrides-" + make_scene_id());
  GameFolder() { std::filesystem::create_directories(dir / "prefabs"); }
  ~GameFolder() {
    std::error_code error;
    std::filesystem::remove_all(dir, error);
  }
  GameFolder(const GameFolder&) = delete;
  GameFolder& operator=(const GameFolder&) = delete;
  void write(const std::string& name, const nlohmann::json& nodes) const {
    GlbFile file;
    file.json = {{"asset", {{"version", "2.0"}}}, {"scene", 0}, {"scenes", {{{"nodes", {0}}}}}, {"nodes", nodes}};
    std::string error;
    REQUIRE_MESSAGE(write_glb(dir / name, file, error), error);
  }
};
nlohmann::json node(const std::string& id, const nlohmann::json& extra) {
  nlohmann::json out = extra;
  out["name"] = id;
  out["extensions"] = {{kSceneIdExtension, {{kSceneIdMember, id}}}};
  return out;
}
// A crate whose lid holds a handle, and a wheel beside it.
void write_crate(const GameFolder& game, double lid_mass = 2) {
  game.write("prefabs/crate.glb",
             {node("crate", {{"children", {1, 3}}}),
              node("lid", {{"translation", {0, 1, 0}}, {"children", {2}}, {"extras", {{"mass", lid_mass}, {"tag", "lid"}}}}),
              node("handle", {{"translation", {0, 0.5, 0}}}), node("wheel", {{"translation", {1, 0, 0}}})});
}
SceneDesc placement(const Overrides& overrides = {}) {
  SceneDesc scene;
  NodeDesc reference;
  reference.name = "Crate";
  reference.stable_id = "placed";
  reference.props[prefabs::kFileProperty] = std::string("prefabs/crate.glb");
  prefabs::write_overrides(reference.props, overrides);
  scene.nodes.push_back(reference);
  return scene;
}
const NodeDesc& named(const SceneDesc& scene, const std::string& name) {
  const int index = scene.find_node(name);
  REQUIRE(index != kNoIndex);
  return scene.nodes[index];
}
NodeDesc child_of(int parent, const std::string& name, const std::string& part = "") {
  NodeDesc child;
  child.name = name;
  child.stable_id = name;
  child.parent = parent;
  if (!part.empty()) child.props[props::kPrefabParent] = part;
  return child;
}
void add_child(SceneDesc& scene, NodeDesc child) {
  scene.nodes[child.parent].children.push_back(static_cast<int>(scene.nodes.size()));
  scene.nodes.push_back(std::move(child));
}
}  // namespace

TEST_CASE("prefab overrides read back what they write, and malformed text names its problem") {
  Overrides overrides;
  overrides["lid"].position = Vec3{1, 2, 3};
  overrides["lid"].props["tag"] = std::string("open");
  overrides["lid"].unset.insert("mass");
  overrides["lid/handle"].removed = true;
  LightDesc light;
  light.type = LightType::Spot;
  light.intensity = 4;
  overrides["lamp"].light = light;
  overrides["eye"].camera = CameraDesc{1, 0.1f, 50};
  PropertyMap props;
  prefabs::write_overrides(props, overrides);
  CHECK(prefabs::read_overrides(props) == overrides);
  prefabs::write_overrides(props, {});
  CHECK(props.count(props::kPrefabOverrides) == 0);
  CHECK_THROWS_WITH_AS(prefabs::parse_overrides(R"({"lid":{"position":[1,2]}})"),
                       "prefab_overrides: position needs 3 numbers", std::runtime_error);
  CHECK_THROWS_AS(prefabs::parse_overrides(R"({"lid":{"colour":1}})"), std::runtime_error);
  CHECK_THROWS_AS(prefabs::parse_overrides("not json"), std::runtime_error);
}

TEST_CASE("reverting one field keeps the others, and an emptied part goes away") {
  Overrides overrides;
  overrides["lid"].position = Vec3{1, 2, 3};
  overrides["lid"].props["tag"] = std::string("open");
  CHECK(prefabs::overridden_fields(overrides["lid"]) == std::vector<std::string>{"position", "props.tag"});
  CHECK(prefabs::revert_field(overrides, "lid", "position"));
  CHECK_FALSE(prefabs::revert_field(overrides, "lid", "scale"));
  CHECK(overrides.at("lid").props.count("tag") == 1);
  CHECK(prefabs::revert_field(overrides, "lid", "props.tag"));
  CHECK(overrides.empty());
  overrides["wheel"].removed = true;
  CHECK(prefabs::revert_field(overrides, "wheel", ""));
  CHECK(overrides.empty());
}

TEST_CASE("an instance keeps its overridden fields while the rest follows the prefab file") {
  GameFolder game;
  write_crate(game);
  Overrides overrides;
  overrides["lid"].position = Vec3{0, 3, 0};
  overrides["lid"].unset.insert("tag");
  const auto scene = placement(overrides);
  auto resolved = prefabs::resolve(scene, game.dir);
  CHECK(named(resolved, "lid").local.position.y == doctest::Approx(3));
  CHECK(named(resolved, "lid").props.count("tag") == 0);
  CHECK(get_number(named(resolved, "lid").props, "mass") == 2);
  CHECK(resolved.nodes[0].props.count(props::kPrefabOverrides) == 0);
  write_crate(game, 7);
  resolved = prefabs::resolve(scene, game.dir);
  CHECK(get_number(named(resolved, "lid").props, "mass") == 7);
  CHECK(named(resolved, "lid").local.position.y == doctest::Approx(3));
  CHECK(named(resolved, "handle").local.position.y == doctest::Approx(0.5));
}

TEST_CASE("a removed part leaves out everything under it") {
  GameFolder game;
  write_crate(game);
  Overrides overrides;
  overrides["lid"].removed = true;
  auto scene = placement(overrides);
  add_child(scene, child_of(0, "Sticker", "lid"));
  const auto resolved = prefabs::resolve(scene, game.dir);
  CHECK(resolved.find_node("lid") == kNoIndex);
  CHECK(resolved.find_node("handle") == kNoIndex);
  CHECK(resolved.find_node("Sticker") == kNoIndex);
  CHECK(resolved.find_node("wheel") != kNoIndex);
  CHECK(resolved.nodes.size() == 2);
  CHECK(resolved.nodes[0].children == std::vector<int>{1});
}

TEST_CASE("a child added under an inherited part hangs from it in the game, in place in the editor") {
  GameFolder game;
  write_crate(game);
  auto scene = placement();
  add_child(scene, child_of(0, "Sticker", "lid/handle"));
  add_child(scene, child_of(0, "Label"));
  const auto played = prefabs::resolve(scene, game.dir);
  const int handle = played.find_node("handle");
  const NodeDesc sticker = named(played, "Sticker");
  CHECK(sticker.parent == handle);
  CHECK(sticker.props.count(props::kPrefabParent) == 0);
  CHECK(played.nodes[played.find_node("Sticker")].parent < played.find_node("Sticker"));
  CHECK(named(played, "Label").parent == 0);
  prefabs::Origins origins;
  const auto previewed = prefabs::resolve(scene, game.dir, {}, &origins);
  CHECK(previewed.find_node("Sticker") == 1);
  CHECK(previewed.nodes[1].parent == 0);
  CHECK(previewed.nodes[1].local.position.y == doctest::Approx(1.5));
  REQUIRE(origins.count(0) == 1);
  CHECK(origins.at(0).parts.count("lid/handle") == 1);
  CHECK(origins.at(0).attached.count(1) == 1);
}

TEST_CASE("overrides reach the parts of a nested prefab by their path") {
  GameFolder game;
  write_crate(game);
  game.write("prefabs/cart.glb",
             {node("cart", {{"children", {1}}}), node("load", {{"extras", {{"prefab_file", "prefabs/crate.glb"}}}})});
  auto scene = placement();
  scene.nodes[0].props[prefabs::kFileProperty] = std::string("prefabs/cart.glb");
  Overrides overrides;
  overrides["load/lid"].props["tag"] = std::string("strapped");
  overrides["load/wheel"].removed = true;
  prefabs::write_overrides(scene.nodes[0].props, overrides);
  const auto resolved = prefabs::resolve(scene, game.dir);
  CHECK(get_string(named(resolved, "lid").props, "tag") == "strapped");
  CHECK(resolved.find_node("wheel") == kNoIndex);
}

TEST_CASE("an override of a part the file no longer has is reported and skipped") {
  GameFolder game;
  write_crate(game);
  Overrides overrides;
  overrides["gone"].removed = true;
  test::LogCapture log;
  const auto resolved = prefabs::resolve(placement(overrides), game.dir);
  CHECK(resolved.nodes.size() == 4);
  CHECK(log.contains("no longer has: gone"));
}

TEST_CASE("overrides saved in a scene file come back when the game loads it") {
  GameFolder game;
  write_crate(game);
  auto document = SceneDocument::create_empty(game.dir / "scene.glb");
  const int crate = document.add_empty("Crate");
  document.set_prop(crate, prefabs::kFileProperty, std::string("prefabs/crate.glb"));
  Overrides overrides;
  overrides["lid"].scale = Vec3{2, 2, 2};
  PropertyMap props;
  prefabs::write_overrides(props, overrides);
  document.set_prop(crate, props::kPrefabOverrides, props.at(props::kPrefabOverrides));
  const int sticker = document.add_empty("Sticker", crate);
  document.set_prop(sticker, props::kPrefabParent, std::string("lid"));
  std::string error;
  REQUIRE_MESSAGE(document.save(error), error);
  const auto loaded = read_game_scene(game.dir);
  REQUIRE(std::holds_alternative<SceneDesc>(loaded));
  const auto& scene = std::get<SceneDesc>(loaded);
  CHECK(named(scene, "lid").local.scale.x == doctest::Approx(2));
  CHECK(named(scene, "Sticker").parent == scene.find_node("lid"));
  auto reopened = SceneDocument::open(game.dir / "scene.glb");
  REQUIRE(std::holds_alternative<SceneDocument>(reopened));
  const auto& again = std::get<SceneDocument>(reopened).desc();
  CHECK(prefabs::read_overrides(again.nodes[again.find_node("Crate")].props) == overrides);
}
