// MCP's scene_prefab tool: a placement's overrides read, set, reverted, applied and unpacked, on
// the saved scene and in the editor's open one (one undo step each).

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>

#include "editor/document/glb_file.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/editor_session.hpp"
#include "project/editor_live_tools.hpp"
#include "project/mcp_tools.hpp"
#include "scene/props.hpp"
#include "scene/scene_ids.hpp"
#include "temp_dirs.hpp"

using namespace redgem;
using namespace redgem::tools;
using mcp::Json;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "scene_prefab";
constexpr float kRaised = 3.0f;

// A game with prefabs/crate.glb (a lid with a handle, and a wheel) placed twice: First and Second.
struct Game {
  fs::path dir = test::temp_root() / ("redgem-prefab-tools-" + make_scene_id());
  Game() {
    fs::create_directories(dir / "prefabs");
    write("prefabs/crate.glb",
          {node("crate", {{"translation", {0, 0.5, 0}}, {"children", {1, 3}}}),
           node("lid", {{"translation", {0, 1, 0}}, {"children", {2}}, {"extras", {{"tag", "lid"}}}}),
           node("handle", {{"translation", {0, 0.5, 0}}}), node("wheel", {{"translation", {1, 0, 0}}})});
    auto document = SceneDocument::create_empty(dir / "scene.glb");
    for (const char* name : {"First", "Second"})
      document.set_prop(document.add_empty(name), props::kPrefabFile, std::string("prefabs/crate.glb"));
    std::string error;
    REQUIRE_MESSAGE(document.save(error), error);
  }
  ~Game() {
    std::error_code ignored;
    fs::remove_all(dir, ignored);
  }
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  static nlohmann::json node(const std::string& id, nlohmann::json extra) {
    extra["name"] = id;
    extra["extensions"] = {{kSceneIdExtension, {{kSceneIdMember, id}}}};
    return extra;
  }
  void write(const std::string& name, const nlohmann::json& nodes) const {
    GlbFile file;
    file.json = {{"asset", {{"version", "2.0"}}}, {"scene", 0}, {"scenes", {{{"nodes", {0}}}}}, {"nodes", nodes}};
    std::string error;
    REQUIRE_MESSAGE(write_glb(dir / name, file, error), error);
  }
  Json call(const Json& args) const { return mcp::invoke({dir, false, {}}, kTool, args); }
  SceneDesc saved(const std::string& file = "scene.glb") const {
    auto opened = SceneDocument::open(dir / file);
    REQUIRE(std::holds_alternative<SceneDocument>(opened));
    return std::get<SceneDocument>(opened).desc();
  }
};

std::string text_of(const Json& result) { return result["content"][0]["text"]; }
bool has(const Json& result, const std::string& part) { return text_of(result).find(part) != std::string::npos; }
bool ok(const Json& result) { return !result["isError"].get<bool>(); }

Json action(const char* name, const char* object = "First") { return {{"action", name}, {"object", object}}; }

Json raise_lid() {
  Json args = action("override");
  args["part"] = "lid";
  args["assignments"] = {"position=0,3,0", "prop.tag="};
  return args;
}

}  // namespace

TEST_CASE("scene_prefab sets, lists and reverts a placement's overrides on the saved scene") {
  const Game game;
  CHECK(has(game.call(action("overrides")), "no overrides"));
  const Json set = game.call(raise_lid());
  CHECK(has(set, "lid position=0,3,0"));
  CHECK(has(set, "lid props.tag=unset"));
  CHECK(has(game.call(action("overrides")), "lid position=0,3,0"));  // (saved: a new call reads it)
  Json revert = action("revert");
  revert["part"] = "lid";
  revert["field"] = "props.tag";
  CHECK_FALSE(has(game.call(revert), "props.tag"));
  CHECK(has(game.call(action("revert")), "no overrides"));
}

TEST_CASE("scene_prefab refuses what is no placement, wrong words, and apply or unpack unconfirmed") {
  const Game game;
  Json named = raise_lid();
  named["assignments"] = {"name=Lid"};
  CHECK_THROWS_AS(game.call(named), std::invalid_argument);
  Json elsewhere = raise_lid();
  elsewhere["part"] = "nothing";
  CHECK_THROWS_AS(game.call(elsewhere), std::invalid_argument);
  Json field_alone = action("revert");
  field_alone["field"] = "position";
  CHECK_THROWS_AS(game.call(field_alone), std::invalid_argument);
  CHECK_THROWS_AS(game.call(action("apply")), std::invalid_argument);
  CHECK_THROWS_AS(game.call(action("unpack")), std::invalid_argument);
  CHECK_THROWS_AS(game.call(action("overrides", "Nobody")), std::invalid_argument);
  Json root = raise_lid();
  root["part"] = ".";
  CHECK_THROWS_WITH_AS(game.call(root), doctest::Contains("scene_edit"), std::invalid_argument);
  Json back = raise_lid();
  back["assignments"] = {"removed=false"};
  CHECK_THROWS_WITH_AS(game.call(back), doctest::Contains("revert"), std::invalid_argument);
}

TEST_CASE("scene_prefab revert of the placement itself takes a field, and leaves the parts' overrides") {
  const Game game;
  game.call(raise_lid());
  Json whole = action("revert");
  whole["part"] = ".";
  CHECK_THROWS_AS(game.call(whole), std::invalid_argument);
  CHECK(has(game.call(action("overrides")), "lid position=0,3,0"));
}

TEST_CASE("scene_prefab apply writes the overrides into the prefab file, and unpack leaves plain objects") {
  const Game game;
  game.call(raise_lid());
  Json apply = action("apply");
  apply["confirm"] = true;
  CHECK(has(game.call(apply), "no overrides"));
  const SceneDesc crate = game.saved("prefabs/crate.glb");
  CHECK(crate.nodes[crate.find_node("lid")].local.position.y == doctest::Approx(kRaised));
  Json unpack = action("unpack", "Second");
  unpack["confirm"] = true;
  CHECK(has(game.call(unpack), "Second"));
  CHECK_THROWS_AS(game.call(action("overrides", "Second")), std::invalid_argument);  // no placement now
  CHECK(game.saved().find_node("lid") != kNoIndex);
}

TEST_CASE("live scene_prefab changes the open scene in one undo step, unsaved, and selects the placement") {
  const Game game;
  auto opened = EditorSession::open(game.dir);
  REQUIRE(std::holds_alternative<std::unique_ptr<EditorSession>>(opened));
  auto& session = *std::get<std::unique_ptr<EditorSession>>(opened);
  const Json set = invoke_editor(session, kTool, raise_lid());
  REQUIRE_MESSAGE(ok(set), set.dump());
  CHECK(session.document().dirty());
  CHECK(session.document().node(session.selection()).name == "First");
  CHECK(has(game.call(action("overrides")), "no overrides"));  // the file is as it was
  session.undo();
  CHECK(has(invoke_editor(session, kTool, action("overrides")), "no overrides"));
  Json unpack = action("unpack", "Second");
  unpack["confirm"] = true;
  REQUIRE(ok(invoke_editor(session, kTool, unpack)));
  CHECK(session.document().node(session.selection()).name == "Second");
  CHECK_FALSE(ok(invoke_editor(session, kTool, action("overrides", "Second"))));
}

TEST_CASE("read-only scene_prefab only lists overrides") {
  mcp::Options options;
  options.read_only = true;
  bool found = false;
  for (const auto& tool : mcp::catalog(options)) {
    if (tool["name"] != kTool) continue;
    found = true;
    CHECK(tool["inputSchema"]["properties"]["action"]["enum"] == Json::array({"overrides"}));
  }
  CHECK(found);
}
