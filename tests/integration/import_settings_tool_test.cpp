// MCP's import_settings tool: an asset's settings listed and set in its sidecar file, and a model
// imported again in the editor's open scene (one undo step an object).

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>

#include "core/config.hpp"
#include "core/files.hpp"
#include "core/import_settings.hpp"
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

constexpr const char* kTool = "import_settings";
constexpr const char* kModel = "models/trees/ball.glb";
constexpr const char* kTexture = "images/grass.png";
constexpr float kBigger = 2.0f;

// A game with a scene, models/trees/ball.glb (one sphere), images/grass.png and notes.txt.
struct Game {
  fs::path dir = test::temp_root() / ("redgem-import-settings-tool-" + make_scene_id());
  Game() {
    fs::create_directories(dir / "models" / "trees");
    fs::create_directories(dir / "images");
    std::string error;
    SceneDocument ball = SceneDocument::create_empty(dir / kModel);
    ball.add_primitive(PrimitiveShape::Sphere, "Ball", redgem::Color{}, kNoIndex);
    REQUIRE_MESSAGE(ball.save(error), error);
    REQUIRE(write_text_file(dir / kTexture, "not read by the tool", error));
    REQUIRE(write_text_file(dir / "notes.txt", "no asset kind", error));
    auto scene = SceneDocument::create_empty(dir / "scene.glb");
    REQUIRE_MESSAGE(scene.save(error), error);
  }
  ~Game() {
    std::error_code ignored;
    fs::remove_all(dir, ignored);
  }
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  Json call(const Json& args, bool read_only = false) const { return mcp::invoke({dir, read_only, {}}, kTool, args); }
  fs::path sidecar(const char* asset) const { return import_settings::sidecar_of(dir / asset); }
};

std::string text_of(const Json& result) { return result["content"][0]["text"]; }
bool has(const Json& result, const std::string& part) { return text_of(result).find(part) != std::string::npos; }
bool ok(const Json& result) { return !result["isError"].get<bool>(); }

Json action(const char* name, const char* asset = kModel) { return {{"action", name}, {"asset", asset}}; }
Json set(const Json& fields, const char* asset = kModel) {
  Json args = action("set", asset);
  args["fields"] = fields;
  return args;
}

std::unique_ptr<EditorSession> open(const Game& game) {
  auto opened = EditorSession::open(game.dir);
  REQUIRE(std::holds_alternative<std::unique_ptr<EditorSession>>(opened));
  return std::move(std::get<std::unique_ptr<EditorSession>>(opened));
}

// The scale the import gave the model's own top object, under the object it sits in.
float imported_scale(EditorSession& session, ObjectId root) {
  const SceneDocument& document = session.document();
  return document.node(document.node(root).children.front()).local.scale.x;
}

}  // namespace

TEST_CASE("import_settings lists an asset's fields with their choices, defaults without a sidecar") {
  const Game game;
  const Json listed = game.call(action("settings"));
  REQUIRE_MESSAGE(ok(listed), listed.dump());
  CHECK(has(listed, std::string("model ") + kModel));
  CHECK(has(listed, "scale = 1"));
  CHECK(has(listed, "up_axis = y (y, z)"));
  CHECK(has(game.call(action("settings", kTexture)), "texture images/grass.png"));
  CHECK_FALSE(fs::exists(game.sidecar(kModel)));
}

TEST_CASE("import_settings set writes every field into the sidecar, or none, and null puts back the default") {
  const Game game;
  const Json saved = game.call(set({{"scale", 0.01}, {"up_axis", "z"}, {"colliders", "box"}}));
  REQUIRE_MESSAGE(ok(saved), saved.dump());
  CHECK(has(saved, std::string("saved ") + kModel + ".import.json"));
  CHECK(has(saved, "action=apply"));
  CHECK(has(game.call(action("settings")), "up_axis = z"));
  // (A field it cannot take refuses the whole call: the scale stays 0.01.)
  CHECK_THROWS_AS(game.call(set({{"scale", 3}, {"colliders", "spheres"}})), std::invalid_argument);
  CHECK_THROWS_AS(game.call(set({{"shininess", 1}})), std::invalid_argument);
  CHECK_THROWS_AS(game.call(set({{"scale", Json::array({1})}})), std::invalid_argument);
  CHECK_THROWS_AS(game.call(set(Json::object())), std::invalid_argument);
  CHECK(has(game.call(action("settings")), "scale = 0.01"));
  REQUIRE(ok(game.call(set({{"up_axis", nullptr}}))));
  CHECK(has(game.call(action("settings")), "up_axis = y"));
  CHECK(has(game.call(action("settings")), "colliders = box"));
  REQUIRE(ok(game.call(set({{"max_size", 512}, {"normal_map", true}}, kTexture))));
  CHECK(has(game.call(action("settings", kTexture)), "max_size = 512"));
}

TEST_CASE("import_settings refuses assets outside the game, files of no kind, misplaced fields, and apply on files") {
  const Game game;
  CHECK_THROWS_AS(game.call(action("settings", "../elsewhere.glb")), std::invalid_argument);
  CHECK_THROWS_AS(game.call(action("settings", "models/missing.glb")), std::invalid_argument);
  CHECK_THROWS_AS(game.call(action("settings", "notes.txt")), std::invalid_argument);
  Json misplaced = action("settings");
  misplaced["fields"] = {{"scale", 2}};
  CHECK_THROWS_AS(game.call(misplaced), std::invalid_argument);
  try {
    game.call(action("apply"));
    FAIL("apply needs an editor");
  } catch (const std::invalid_argument& error) {
    CHECK(std::string(error.what()).find("--editor-bridge") != std::string::npos);
  }
}

TEST_CASE("read-only import_settings only lists settings") {
  const Game game;
  CHECK(ok(game.call(action("settings"), true)));
  CHECK_THROWS_AS(game.call(set({{"scale", 2}}), true), std::invalid_argument);
  CHECK_FALSE(fs::exists(game.sidecar(kModel)));
}

TEST_CASE("live import_settings apply imports a model again into each copy of the open scene, one undo step each") {
  const Game game;
  auto session = open(game);
  std::string error;
  const ObjectId first = session->import_model(game.dir / kModel, Vec3{}, error);
  REQUIRE_MESSAGE(first != kNoObject, error);
  session->document().set_name(first, "Tree");
  CHECK(imported_scale(*session, first) == doctest::Approx(1.0f));

  REQUIRE(ok(invoke_editor(*session, kTool, set({{"scale", kBigger}}))));
  CHECK(fs::exists(game.sidecar(kModel)));
  const Json applied = invoke_editor(*session, kTool, action("apply"));
  REQUIRE_MESSAGE(ok(applied), applied.dump());
  CHECK(has(applied, "1 object(s)"));
  const ObjectId tree = session->scene().find("Tree");
  REQUIRE(tree != kNoObject);
  CHECK(imported_scale(*session, tree) == doctest::Approx(kBigger));
  CHECK(session->document().dirty());
  session->undo();
  CHECK(imported_scale(*session, session->scene().find("Tree")) == doctest::Approx(1.0f));
}

TEST_CASE("live import_settings apply says when no object came from the model, and takes only models of models/") {
  const Game game;
  auto session = open(game);
  CHECK(has(invoke_editor(*session, kTool, action("apply")), "no object of the open scene"));
  CHECK_FALSE(ok(invoke_editor(*session, kTool, action("apply", kTexture))));
  CHECK_FALSE(ok(invoke_editor(*session, kTool, action("apply", "scene.glb"))));
  CHECK(has(invoke_editor(*session, kTool, action("settings")), "scale = 1"));
}
