#include <doctest/doctest.h>

#include <filesystem>
#include <nlohmann/json.hpp>

#include "app/scene_files.hpp"
#include "editor/document/glb_file.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/prefab_instances.hpp"
#include "project/scene_command.hpp"
#include "scene/prefabs.hpp"
#include "scene/props.hpp"
#include "scene/scene_ids.hpp"
#include "test_support.hpp"

using namespace redgem;
using prefabs::Overrides;
using prefabs::PartOverride;

namespace {
struct Game {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem-instances-" + make_scene_id());
  Game() {
    std::filesystem::create_directories(dir / "prefabs");
    write_crate();
    write("prefabs/cart.glb", {node("cart", {{"children", {1}}}),
                               node("load", {{"extras", {{"prefab_file", "prefabs/crate.glb"}}}})});
  }
  ~Game() {
    std::error_code error;
    std::filesystem::remove_all(dir, error);
  }
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;
  static nlohmann::json node(const std::string& id, const nlohmann::json& extra) {
    nlohmann::json out = extra;
    out["name"] = id;
    out["extensions"] = {{kSceneIdExtension, {{kSceneIdMember, id}}}};
    return out;
  }
  void write(const std::string& name, const nlohmann::json& nodes) const {
    GlbFile file;
    file.json = {{"asset", {{"version", "2.0"}}}, {"scene", 0}, {"scenes", {{{"nodes", {0}}}}}, {"nodes", nodes}};
    std::string error;
    REQUIRE_MESSAGE(write_glb(dir / name, file, error), error);
  }
  void write_crate() const {
    write("prefabs/crate.glb",
          {node("crate", {{"translation", {0, 0.5, 0}}, {"children", {1, 3}}}),
           node("lid", {{"translation", {0, 1, 0}}, {"children", {2}}, {"extras", {{"mass", 2}, {"tag", "lid"}}}}),
           node("handle", {{"translation", {0, 0.5, 0}}}), node("wheel", {{"translation", {1, 0, 0}}})});
  }
  // A scene with two placements of `file`; the first is returned open.
  SceneDocument scene(const std::string& file = "prefabs/crate.glb") const {
    auto document = SceneDocument::create_empty(dir / "scene.glb");
    for (const char* name : {"First", "Second"}) {
      const int placed = document.add_empty(name);
      document.set_prop(placed, props::kPrefabFile, file);
    }
    std::string error;
    REQUIRE_MESSAGE(document.save(error), error);
    return document;
  }
  SceneDesc played() const {
    auto loaded = read_game_scene(dir);
    REQUIRE(std::holds_alternative<SceneDesc>(loaded));
    return std::get<SceneDesc>(loaded);
  }
  SceneDesc prefab(const std::string& file = "prefabs/crate.glb") const {
    auto opened = SceneDocument::open(dir / file);
    REQUIRE(std::holds_alternative<SceneDocument>(opened));
    return std::get<SceneDocument>(opened).desc();
  }
};
PartOverride moved_to(float y) {
  PartOverride change;
  change.position = Vec3{0, y, 0};
  return change;
}
int find(const SceneDocument& document, const std::string& name) {
  const int index = document.desc().find_node(name);
  REQUIRE(index != kNoIndex);
  return index;
}
// The `name` under the placement called `placement` in a resolved scene.
const NodeDesc& part_of(const SceneDesc& scene, const std::string& placement, const std::string& name) {
  const int root = scene.find_node(placement);
  REQUIRE(root != kNoIndex);
  for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
    int up = scene.nodes[i].parent;
    while (up != kNoIndex && up != root) up = scene.nodes[up].parent;
    if (up == root && scene.nodes[i].name == name) return scene.nodes[i];
  }
  FAIL("no part " << name << " under " << placement);
  return scene.nodes.front();
}
}  // namespace

TEST_CASE("a placement's overrides are set, listed and reverted one field at a time, with undo") {
  Game game;
  auto scene = game.scene();
  const int first = find(scene, "First");
  std::string error;
  auto change = moved_to(3);
  change.props["tag"] = std::string("open");
  REQUIRE_MESSAGE(prefab_instances::set_override(scene, first, "lid", change, error), error);
  scene.set_prop(first, "health", 5.0);
  const auto listed = prefab_instances::fields(scene, first);
  REQUIRE(listed.size() == 3);
  CHECK(listed[0].path.empty());
  CHECK(listed[0].name == "props.health");
  CHECK(listed[1].name == "position");
  CHECK(listed[1].value == "0,3,0");
  CHECK(listed[2].value == "open");
  REQUIRE_MESSAGE(prefab_instances::revert(scene, first, "lid", "position", error), error);
  CHECK(prefab_instances::overrides_of(scene, first).at("lid").props.count("tag") == 1);
  CHECK_FALSE(prefab_instances::revert(scene, first, "lid", "scale", error));
  CHECK_FALSE(error.empty());
  CHECK(scene.undo());
  CHECK(prefab_instances::overrides_of(scene, first).at("lid").position.has_value());
  REQUIRE_MESSAGE(prefab_instances::revert(scene, first, "", "", error), error);
  CHECK(prefab_instances::fields(scene, first).empty());
  CHECK(scene.undo());
  CHECK(prefab_instances::fields(scene, first).size() == 3);
}

TEST_CASE("applying a placement writes its overrides into the prefab file for every placement") {
  Game game;
  auto scene = game.scene();
  const int first = find(scene, "First");
  std::string error;
  auto change = moved_to(4);
  change.unset.insert("tag");
  REQUIRE(prefab_instances::set_override(scene, first, "lid", change, error));
  PartOverride gone;
  gone.removed = true;
  REQUIRE(prefab_instances::set_override(scene, first, "wheel", gone, error));
  scene.set_prop(first, "health", 5.0);
  const int sticker = scene.add_empty("Sticker", first);
  scene.set_prop(sticker, props::kPrefabParent, std::string("lid"));
  REQUIRE_MESSAGE(prefab_instances::apply(scene, first, game.dir, error), error);
  CHECK(prefab_instances::fields(scene, first).empty());
  CHECK(scene.node(find(scene, "First")).children.empty());
  REQUIRE_MESSAGE(scene.save(error), error);
  const auto file = game.prefab();
  CHECK(file.find_node("wheel") == kNoIndex);
  CHECK(file.nodes[file.find_node("lid")].local.position.y == doctest::Approx(4));
  CHECK(file.nodes[file.find_node("Sticker")].parent == file.find_node("lid"));
  CHECK(get_number(file.nodes[0].props, "health") == 5.0);
  const auto played = game.played();
  CHECK(part_of(played, "Second", "lid").local.position.y == doctest::Approx(4));
  CHECK(part_of(played, "Second", "lid").props.count("tag") == 0);
  CHECK(part_of(played, "Second", "Sticker").name == "Sticker");
  CHECK(scene.undo());
  CHECK(prefab_instances::overrides_of(scene, find(scene, "First")).count("lid") == 1);
}

TEST_CASE("applying an override of a nested part writes it onto the nested placement in the file") {
  Game game;
  auto scene = game.scene("prefabs/cart.glb");
  const int first = find(scene, "First");
  std::string error;
  REQUIRE(prefab_instances::set_override(scene, first, "load/lid", moved_to(6), error));
  REQUIRE_MESSAGE(prefab_instances::apply(scene, first, game.dir, error), error);
  const auto cart = game.prefab("prefabs/cart.glb");
  const auto nested = prefabs::read_overrides(cart.nodes[cart.find_node("load")].props);
  REQUIRE(nested.count("lid") == 1);
  CHECK(nested.at("lid").position->y == doctest::Approx(6));
  CHECK(game.prefab().nodes[game.prefab().find_node("lid")].local.position.y == doctest::Approx(1));
}

TEST_CASE("unpacking a placement leaves plain objects with its overrides, in one undo step") {
  Game game;
  auto scene = game.scene();
  int first = find(scene, "First");
  Transform placed;
  placed.position = {5, 0, 0};
  scene.set_local(first, placed);
  std::string error;
  REQUIRE(prefab_instances::set_override(scene, first, "lid", moved_to(2), error));
  PartOverride gone;
  gone.removed = true;
  REQUIRE(prefab_instances::set_override(scene, first, "wheel", gone, error));
  const int sticker = scene.add_empty("Sticker", first);
  scene.set_prop(sticker, props::kPrefabParent, std::string("lid/handle"));
  const auto before = scene.node_count();
  const int root = prefab_instances::unpack(scene, find(scene, "First"), game.dir, error);
  REQUIRE_MESSAGE(root != kNoIndex, error);
  const auto& desc = scene.desc();
  CHECK(desc.nodes[root].name == "First");
  CHECK(desc.nodes[root].props.count(props::kPrefabFile) == 0);
  CHECK(desc.nodes[root].local.position.x == doctest::Approx(5));
  CHECK(desc.nodes[root].local.position.y == doctest::Approx(0.5));
  CHECK(desc.find_node("wheel") == kNoIndex);
  const int lid = desc.find_node("lid");
  CHECK(desc.nodes[lid].local.position.y == doctest::Approx(2));
  const int moved = desc.find_node("Sticker");
  CHECK(desc.nodes[moved].parent == desc.find_node("handle"));
  CHECK(desc.nodes[moved].props.count(props::kPrefabParent) == 0);
  CHECK(scene.undo());
  CHECK(scene.node_count() == before);
  CHECK(prefab_instances::is_placement(scene, find(scene, "First")));
}

TEST_CASE("redgem scene overrides, override, revert, apply and unpack work on a saved scene") {
  Game game;
  game.scene();
  const auto run = [&game](std::vector<std::string> words) {
    words.insert(words.begin() + 1, game.dir.string());
    return tools::run_scene_command(words);
  };
  auto result = run({"override", "First", "lid", "position=0,3,0", "prop.tag="});
  REQUIRE_MESSAGE(result.exit_code == 0, result.err);
  result = run({"overrides", "First"});
  CHECK(result.out == "lid position=0,3,0\nlid props.tag=unset\n");
  CHECK(run({"override", "First", "lid", "name=Lid"}).exit_code != 0);
  CHECK(run({"override", "First", "nothing", "position=0,1,0"}).exit_code != 0);
  CHECK(run({"overrides", "Second"}).out.find("no overrides") != std::string::npos);
  REQUIRE(run({"revert", "First", "lid", "props.tag"}).exit_code == 0);
  CHECK(run({"overrides", "First"}).out == "lid position=0,3,0\n");
  REQUIRE(run({"apply", "First"}).exit_code == 0);
  CHECK(part_of(game.played(), "Second", "lid").local.position.y == doctest::Approx(3));
  REQUIRE(run({"unpack", "Second"}).exit_code == 0);
  CHECK(game.played().find_node("Second") != kNoIndex);
  CHECK(run({"apply", "Second"}).exit_code != 0);
}

TEST_CASE("unpacking keeps every added child, skips stale overrides, and keeps children of removed parts") {
  Game game;
  auto scene = game.scene();
  const int first = find(scene, "First");
  std::string error;
  PartOverride gone;
  gone.removed = true;
  REQUIRE(prefab_instances::set_override(scene, first, "lid", gone, error));
  REQUIRE(prefab_instances::set_override(scene, first, "missing", moved_to(9), error));
  scene.add_empty("Plain", first);
  const int hung = scene.add_empty("Hung", find(scene, "First"));
  scene.set_prop(hung, props::kPrefabParent, std::string("lid"));
  scene.add_empty("Other", find(scene, "First"));
  test::LogCapture log;
  const int root = prefab_instances::unpack(scene, find(scene, "First"), game.dir, error);
  REQUIRE_MESSAGE(root != kNoIndex, error);
  CHECK(log.contains("missing"));
  for (const char* name : {"Plain", "Hung", "Other"}) {
    CHECK(scene.node(find(scene, name)).parent == find(scene, "First"));
  }
  CHECK(scene.desc().find_node("lid") == kNoIndex);
}

TEST_CASE("an override set back to the file's value is no longer an override") {
  Game game;
  game.scene();
  const auto run = [&game](std::vector<std::string> words) {
    words.insert(words.begin() + 1, game.dir.string());
    return tools::run_scene_command(words);
  };
  REQUIRE(run({"override", "First", "lid", "prop.color=red"}).exit_code == 0);
  CHECK(run({"overrides", "First"}).out == "lid props.color=red\n");
  REQUIRE(run({"override", "First", "lid", "prop.color="}).exit_code == 0);
  CHECK(run({"overrides", "First"}).out.find("no overrides") != std::string::npos);
  REQUIRE(run({"override", "First", "lid", "position=0,1,0"}).exit_code == 0);
  CHECK(run({"overrides", "First"}).out.find("no overrides") != std::string::npos);
}
