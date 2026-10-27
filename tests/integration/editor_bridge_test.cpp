#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <stdexcept>
#include <thread>

#include "core/files.hpp"
#include "editor/editor_session.hpp"
#include "project/cli.hpp"
#include "project/editor_bridge.hpp"
#include "project/editor_bridge_wire.hpp"
#include "project/editor_live_tools.hpp"
#include "project/mcp_tools.hpp"
#include "project/scene_lookup.hpp"

using namespace redgem;
using namespace redgem::tools;
using mcp::Json;
namespace fs = std::filesystem;
namespace {
class Game {
 public:
  Game() {
    fs::remove_all(root);
    fs::create_directories(root);
    fs::copy(fs::path(REDGEM_PLATFORMER_DIR), path, fs::copy_options::recursive);
    auto opened = EditorSession::open(path);
    if (const auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
    session = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  }
  ~Game() { fs::remove_all(root); }
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;
  fs::path root = fs::temp_directory_path() / "redgem-editor-bridge-test";
  fs::path path = root / "game";
  std::unique_ptr<EditorSession> session;
};

int coin(const Game& game) { return find_object(game.session->scene_desc(), "Coin1").index; }
Json set(const std::vector<std::string>& words) {
  return {{"action", "set"}, {"object", "Coin1"}, {"assignments", words}};
}
bool ok(const Json& result) { return !result.at("isError").get<bool>(); }

Json exchange(Game& game, EditorBridge& server, const std::string& name, const Json& args,
              const fs::path& connection = {}, const fs::path& client_game = {}) {
  const auto file = connection.empty() ? server.connection_file() : connection;
  const auto root = client_game.empty() ? game.path : client_game;
  const mcp::Options options{root, false, {}, file};
  auto future = std::async(std::launch::async, [&] { return mcp::invoke(options, name, args); });
  const auto main = std::this_thread::get_id();
  while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
    server.tick([&](const std::string& tool, const Json& values) {
      CHECK(std::this_thread::get_id() == main);
      return invoke_editor(*game.session, tool, values);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return future.get();
}
}  // namespace

TEST_CASE("live editor node edits are one undo step, update the preview and leave the file alone") {
  Game game;
  auto& session = *game.session;
  const auto index = coin(game);
  const auto original = session.document().node(index).local;
  const auto time = fs::last_write_time(game.path / "scene.glb");
  CHECK(ok(invoke_editor(session, "scene_edit", set({"position=1,2,3", "prop.bridge=true"}))));
  CHECK(session.document().dirty());
  CHECK(session.document().node(index).local.position.x == 1);
  CHECK(session.scene().at(static_cast<ObjectId>(index)).local.position.y == 2);
  CHECK(fs::last_write_time(game.path / "scene.glb") == time);
  CHECK(ok(invoke_editor(session, "editor", {{"action", "undo"}})));
  CHECK_FALSE(session.document().dirty());
  CHECK(session.document().node(index).props.count("bridge") == 0);
  CHECK(session.document().node(index).local.position.x == original.position.x);
  CHECK_FALSE(session.document().can_undo());
  CHECK(ok(invoke_editor(session, "editor", {{"action", "redo"}})));
  CHECK(session.document().node(index).props.count("bridge") == 1);
}

TEST_CASE("a failed live edit rolls back earlier fields and preserves existing redo") {
  Game game;
  auto& doc = game.session->document();
  const auto index = coin(game);
  doc.set_prop(index, "earlier", true);
  game.session->undo();
  REQUIRE(doc.can_redo());
  const auto original = doc.node(index).local.position;
  const auto reply = invoke_editor(*game.session, "scene_edit", set({"position=7,8,9", "position=bad"}));
  CHECK_FALSE(ok(reply));
  CHECK(doc.node(index).local.position.x == original.x);
  CHECK_FALSE(doc.dirty());
  CHECK_FALSE(doc.can_undo());
  CHECK(doc.can_redo());
  CHECK(doc.redo());
  CHECK(doc.node(index).props.count("earlier") == 1);
}

TEST_CASE("node edit exceptions and no-ops preserve versions and undo history") {
  Game game;
  auto& doc = game.session->document();
  const auto index = coin(game);
  std::string error;
  CHECK_THROWS_AS(doc.edit_existing_nodes(
                      [&]() -> std::string {
                        doc.set_prop(index, "temporary", true);
                        throw std::runtime_error("stopped");
                      },
                      error),
                  std::runtime_error);
  CHECK(doc.node(index).props.count("temporary") == 0);
  CHECK_FALSE(doc.can_undo());
  CHECK_FALSE(doc.dirty());
  CHECK(doc.edit_existing_nodes(
      [&] {
        doc.set_prop(index, "temporary", true);
        doc.remove_prop(index, "temporary");
        return std::string();
      },
      error));
  CHECK_FALSE(doc.can_undo());
  CHECK_FALSE(doc.dirty());
  doc.set_prop(index, "after", true);
  CHECK(doc.can_undo());
}

TEST_CASE("live editor inspection sees unsaved state and explicit save persists it") {
  Game game;
  CHECK(ok(invoke_editor(*game.session, "scene_edit", set({"position=1,2,3"}))));
  const auto detail = invoke_editor(*game.session, "scene_get", {{"object", "Coin1"}});
  CHECK(detail.dump().find("1,2,3") != std::string::npos);
  CHECK(ok(invoke_editor(*game.session, "editor", {{"action", "select"}, {"object", "Coin1"}})));
  CHECK(game.session->selection() == static_cast<ObjectId>(coin(game)));
  CHECK(ok(invoke_editor(*game.session, "editor", {{"action", "save"}})));
  CHECK_FALSE(game.session->document().dirty());
  auto reopened = EditorSession::open(game.path);
  REQUIRE(std::holds_alternative<std::unique_ptr<EditorSession>>(reopened));
  CHECK(std::get<std::unique_ptr<EditorSession>>(reopened)->document().node(coin(game)).local.position.x == 1);
}

TEST_CASE("live editing rejects gestures, another scene, creation and playing") {
  Game game;
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit", set({"position=1,2,3"}), true)));
  auto other = set({"position=1,2,3"});
  other["scene"] = "another";
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit", other)));
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit", {{"action", "add"}, {"object", "New"}})));
  ScriptedInputSource input;
  std::string error;
  REQUIRE(game.session->play({input}, error));
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit", set({"position=1,2,3"}))));
  CHECK_FALSE(game.session->document().dirty());
  game.session->stop();
}

TEST_CASE("editor bridge authenticates real loopback calls and runs edits on the polling thread") {
  Game game;
  EditorBridge server(game.path, game.root / "private");
  const auto file = server.connection_file();
  REQUIRE(fs::exists(file));
#ifndef _WIN32
  const auto permissions = fs::status(file).permissions();
  CHECK((permissions & fs::perms::others_all) == fs::perms::none);
  CHECK((permissions & fs::perms::group_all) == fs::perms::none);
#endif
  CHECK(ok(exchange(game, server, "scene_edit", set({"position=1,2,3", "prop.live=true"}))));
  CHECK(game.session->document().dirty());
  CHECK(ok(exchange(game, server, "editor", {{"action", "undo"}})));
  CHECK_FALSE(game.session->document().dirty());
  CHECK(ok(exchange(game, server, "events", {{"limit", 1}})));
}

TEST_CASE("wrong credentials cannot mutate the editor and credentials never enter errors") {
  Game game;
  EditorBridge server(game.path, game.root / "private");
  std::string error;
  auto text = read_text_file(server.connection_file(), error);
  REQUIRE(text);
  Json descriptor = Json::parse(*text);
  const auto token = descriptor["token"].get<std::string>();
  descriptor["token"] = std::string(64, 'x');
  const auto fake = game.root / "wrong.json";
  REQUIRE(write_text_file(fake, descriptor.dump(), error));
  const auto result = exchange(game, server, "scene_edit", set({"position=1,2,3"}), fake);
  CHECK_FALSE(ok(result));
  CHECK_FALSE(game.session->document().dirty());
  CHECK(result.dump().find(token) == std::string::npos);
  CHECK_FALSE(ok(call_editor(game.root, server.connection_file(), "editor", {{"action", "status"}})));
}

TEST_CASE("editor connection credentials are removed when the bridge closes") {
  Game game;
  fs::path file;
  {
    EditorBridge server(game.path, game.root / "private");
    file = server.connection_file();
    CHECK(fs::exists(file));
  }
  CHECK_FALSE(fs::exists(file));
}

TEST_CASE("live MCP catalog exposes status only under read-only and narrows edit actions") {
  mcp::Options options;
  options.editor_connection = "connection.json";
  options.read_only = true;
  const auto read = mcp::catalog(options);
  CHECK(read.size() == 6);
  for (const auto& tool : read) {
    if (tool["name"] == "editor") CHECK(tool["inputSchema"]["properties"]["action"]["enum"] == Json::array({"status"}));
  }
  options.read_only = false;
  for (const auto& tool : mcp::catalog(options)) {
    if (tool["name"] == "scene_edit")
      CHECK(tool["inputSchema"]["properties"]["action"]["enum"] == Json::array({"set"}));
  }
}

TEST_CASE("the editor bridge is opt-in and cannot be enabled for a player or build") {
  CHECK_FALSE(parse_cli({"games/demo"}).options.editor_bridge);
  const auto opened = parse_cli({"games/demo", "--editor-bridge"});
  REQUIRE(opened.ok);
  CHECK(opened.options.editor_bridge);
  CHECK_FALSE(parse_cli({"games/demo", "--editor-bridge", "--play"}).ok);
  CHECK_FALSE(parse_cli({"build", "games/demo", "out", "--editor-bridge"}).ok);
}

TEST_CASE("the editor checks the actual game even when a descriptor claims another root") {
  Game game;
  EditorBridge server(game.path, game.root / "private");
  std::string error;
  const auto original = read_text_file(server.connection_file(), error);
  REQUIRE(original);
  Json descriptor = Json::parse(*original);
  descriptor["game"] = game.root.generic_u8string();
  const auto fake = game.root / "other-game.json";
  REQUIRE(write_text_file(fake, descriptor.dump(), error));
  const auto result = exchange(game, server, "scene_edit", set({"position=1,2,3"}), fake, game.root);
  CHECK_FALSE(ok(result));
  CHECK_FALSE(game.session->document().dirty());
}

TEST_CASE("invalid descriptors and oversized editor requests fail without leaking credentials") {
  Game game;
  EditorBridge server(game.path, game.root / "private");
  std::string error;
  const auto original = read_text_file(server.connection_file(), error);
  REQUIRE(original);
  const std::string token = Json::parse(*original)["token"];
  const auto bad = game.root / "invalid.json";
  REQUIRE(write_text_file(bad, "{\"token\":\"" + token + "\"", error));
  const auto result = call_editor(game.path, bad, "editor", {{"action", "status"}});
  CHECK_FALSE(ok(result));
  CHECK(result.dump().find(token) == std::string::npos);
  const auto oversized = call_editor(game.path, server.connection_file(), "scene_edit",
                                     {{"value", std::string(bridge::kMaxRequest + 1, 'x')}});
  CHECK_FALSE(ok(oversized));
  CHECK_FALSE(game.session->document().dirty());
}

TEST_CASE("live commands do not overwrite an external change pending in the editor") {
  Game game;
  CHECK(ok(invoke_editor(*game.session, "scene_edit", set({"position=1,2,3"}))));
  const auto file = game.path / "scene.glb";
  const auto changed = fs::last_write_time(file) + std::chrono::seconds(1);
  fs::last_write_time(file, changed);
  CHECK_FALSE(game.session->reload_if_changed_on_disk());
  REQUIRE(game.session->external_change_pending());
  CHECK_FALSE(ok(invoke_editor(*game.session, "editor", {{"action", "save"}})));
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit", set({"position=7,8,9"}))));
  CHECK(fs::last_write_time(file) == changed);
  CHECK(game.session->document().dirty());
}

TEST_CASE("live MCP rejects mutation actions in read-only mode before connecting") {
  Game game;
  mcp::Options options{game.path, true, {}, "missing-connection.json"};
  CHECK_THROWS_AS(mcp::invoke(options, "editor", {{"action", "save"}}), std::invalid_argument);
  CHECK_THROWS_AS(mcp::invoke(options, "scene_edit", set({"position=1,2,3"})), std::invalid_argument);
  CHECK_FALSE(game.session->document().dirty());
}

TEST_CASE("editor commands reject unrelated object arguments before touching undo history") {
  Game game;
  REQUIRE(ok(invoke_editor(*game.session, "scene_edit", set({"position=1,2,3"}))));
  CHECK_FALSE(ok(invoke_editor(*game.session, "editor", {{"action", "undo"}, {"object", "Coin1"}})));
  CHECK_FALSE(ok(invoke_editor(*game.session, "editor", {{"action", "select"}})));
  CHECK(game.session->document().dirty());
  CHECK(game.session->document().can_undo());
}
