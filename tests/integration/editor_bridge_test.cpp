#include <doctest/doctest.h>
#include <mruby.h>

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
  CHECK_THROWS_AS(doc.edit_nodes(
                      [&]() -> std::string {
                        doc.set_prop(index, "temporary", true);
                        throw std::runtime_error("stopped");
                      },
                      error),
                  std::runtime_error);
  CHECK(doc.node(index).props.count("temporary") == 0);
  CHECK_FALSE(doc.can_undo());
  CHECK_FALSE(doc.dirty());
  CHECK(doc.edit_nodes(
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

TEST_CASE("live editing rejects gestures, another scene and playing") {
  Game game;
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit", set({"position=1,2,3"}), true)));
  auto other = set({"position=1,2,3"});
  other["scene"] = "another";
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit", other)));
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit", {{"action", "add"}, {"object", "../New"}})));
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
    if (tool["name"] == "editor")
      CHECK(tool["inputSchema"]["properties"]["action"]["enum"] == Json::array({"status", "capture"}));
  }
  options.read_only = false;
  for (const auto& tool : mcp::catalog(options)) {
    if (tool["name"] == "scene_edit")
      CHECK(tool["inputSchema"]["properties"]["action"]["enum"] == Json::array({"set", "add", "remove"}));
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

TEST_CASE("live creation and removal are atomic single undo steps with stable identities") {
  Game game;
  auto& session = *game.session;
  const auto original = session.document().node_count();
  REQUIRE(ok(invoke_editor(
      session, "scene_edit",
      {{"action", "add"}, {"object", "BridgeObject"}, {"parent", "Ground"}, {"assignments", {"position=1,2,3"}}})));
  const auto found = find_object(session.scene_desc(), "BridgeObject");
  REQUIRE(found.index != kNoIndex);
  const std::string identity = session.document().node(found.index).stable_id;
  CHECK(session.document().node_count() == original + 1);
  session.undo();
  CHECK(session.document().node_count() == original);
  CHECK_FALSE(session.document().dirty());
  session.redo();
  CHECK(session.document().node(find_object(session.scene_desc(), "BridgeObject").index).stable_id == identity);
  CHECK_FALSE(ok(invoke_editor(session, "scene_edit", {{"action", "remove"}, {"object", "BridgeObject"}})));
  REQUIRE(
      ok(invoke_editor(session, "scene_edit", {{"action", "remove"}, {"object", "BridgeObject"}, {"confirm", true}})));
  CHECK(session.document().node_count() == original);
  session.undo();
  CHECK(session.document().node(find_object(session.scene_desc(), "BridgeObject").index).stable_id == identity);
}

TEST_CASE("failed live creation leaves neither an object nor an undo step") {
  Game game;
  const auto count = game.session->document().node_count();
  CHECK_FALSE(ok(invoke_editor(*game.session, "scene_edit",
                               {{"action", "add"}, {"object", "Bad"}, {"assignments", {"position=bad"}}})));
  CHECK(game.session->document().node_count() == count);
  CHECK_FALSE(game.session->document().can_undo());
  CHECK_FALSE(game.session->document().dirty());
}

TEST_CASE("single stepping only advances paused games and leaves the editor document unchanged") {
  Game game;
  auto& session = *game.session;
  CHECK_FALSE(session.step());
  ScriptedInputSource input;
  std::string error;
  REQUIRE(session.play({input}, error));
  CHECK_FALSE(session.step());
  session.pause(true);
  const auto installed = session.runtime()->scripts().eval(
      "$mcp_steps = 0; class StepProbe < Behaviour; def update(_dt); $mcp_steps += 1; end; end; "
      "Scene.find('Player').add_component(StepProbe)",
      "step_test.rb");
  REQUIRE_MESSAGE(installed.ok, installed.error);
  REQUIRE(session.step());
  const auto counted = session.runtime()->scripts().eval("$mcp_steps", "step_test.rb");
  REQUIRE(counted.ok);
  CHECK(mrb_fixnum(counted.value) == 1);
  CHECK(session.play_state() == PlayState::Paused);
  CHECK_FALSE(session.document().dirty());
  session.stop();
  CHECK_FALSE(session.step());
}

TEST_CASE("editor bridge deadlines use the supplied clock without changing the production budget") {
  Game game;
  bridge::Clock::time_point now{};
  EditorBridge server(game.path, game.root / "bridge", [&] { return now; });
  SUBCASE("a client stops waiting even when the editor never pumps") {
    std::size_t readings = 0;
    const auto result = call_editor(game.path, server.connection_file(), "editor", {{"action", "status"}}, [&] {
      ++readings;
      now += std::chrono::seconds(1);
      return now;
    });
    CHECK(result["isError"] == true);
    CHECK(result["content"][0]["text"].get<std::string>().find("timed out") != std::string::npos);
    CHECK(readings > 0);
  }
  SUBCASE("an idle server peer expires before it supplies a complete request") {
    const auto address = bridge::read_address(game.path, server.connection_file());
    std::string error;
    auto socket = net::Socket::connect_to(net::kThisMachine, address.port, error);
    REQUIRE(socket.valid());
    constexpr int kConnectPolls = 100;
    for (int attempt = 0; attempt < kConnectPolls && socket.connect_status() == net::IoStatus::WouldBlock; ++attempt)
      net::Socket::wait({{&socket, true}}, bridge::kPollSeconds);
    REQUIRE(socket.connect_status() == net::IoStatus::Ok);
    const mcp::Invoke unused = [](const std::string&, const Json&) { return Json::object(); };
    server.tick(unused);
    now += bridge::kTimeout + std::chrono::seconds(1);
    server.tick(unused);
    bridge::Bytes received;
    net::Socket::wait({{&socket, false}}, bridge::kPollSeconds);
    CHECK(socket.read(received, bridge::kMaxReply) == net::IoStatus::Closed);
    CHECK(received.empty());
  }
}

namespace {
// Calls through `options` while the editor polls `server`, as exchange does with a fixed file.
Json exchange_with(Game& game, EditorBridge& server, const mcp::Options& options, const std::string& name,
                   const Json& args) {
  auto future = std::async(std::launch::async, [&] { return mcp::invoke(options, name, args); });
  while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
    server.tick([&](const std::string& tool, const Json& values) { return invoke_editor(*game.session, tool, values); });
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return future.get();
}

std::string text(const Json& result) { return result["content"][0]["text"]; }
}  // namespace

TEST_CASE("editor discovery finds this game's running editor and skips other games and stale files") {
  Game game;
  const auto root = game.root / "private";
  EditorBridge server(game.path, root);
  CHECK(editor_connections(game.path, root) == std::vector<fs::path>{server.connection_file()});
  CHECK(editor_connections(REDGEM_PLATFORMER_DIR, root).empty());
  std::string error;
  Json stale = Json::parse(*read_text_file(server.connection_file(), error));
  stale["port"] = 1;  // nothing listens there: an editor that crashed
  fs::create_directories(root / "stale");
  REQUIRE(write_text_file(root / "stale" / "connection.json", stale.dump(), error));
  CHECK(editor_connections(game.path, root).size() == 2);
  CHECK_FALSE(try_editor(game.path, root / "stale" / "connection.json", "editor", {{"action", "status"}}));
  const auto refused = call_editor(game.path, root / "stale" / "connection.json", "editor", {{"action", "status"}});
  CHECK(text(refused).find("--editor-bridge") != std::string::npos);
}

TEST_CASE("--editor auto edits the running editor live and falls back to files without one") {
  Game game;
  const auto root = game.root / "private";
  const mcp::Options options{game.path, false, {}, mcp::kDiscoverEditor, root};
  {
    EditorBridge server(game.path, root);
    CHECK(ok(exchange_with(game, server, options, "scene_edit", set({"position=1,2,3"}))));
    CHECK(game.session->document().dirty());  // live: unsaved in the editor
  }
  const auto status = mcp::invoke(options, "editor", {{"action", "status"}});
  CHECK_FALSE(ok(status));
  CHECK(text(status).find("--editor-bridge") != std::string::npos);
  CHECK(ok(mcp::invoke(options, "scene_edit", set({"position=4,5,6"}))));
  CHECK(text(mcp::invoke(options, "scene_get", {{"object", "Coin1"}})).find("position: 4,5,6") != std::string::npos);
}

TEST_CASE("live game_query asks the editor's running game, not the edited document") {
  Game game;
  auto& session = *game.session;
  const Json inspect{{"action", "inspect"}, {"object", "Player"}};
  CHECK(text(invoke_editor(session, "game_query", inspect)).find("not playing") != std::string::npos);
  ScriptedInputSource input;
  std::string error;
  REQUIRE(session.play({input}, error));
  session.runtime()->scene().set_world_position(static_cast<ObjectId>(find_object(session.scene_desc(), "Crate1").index),
                                                Vec3{9, 9, 9});
  const auto crate = invoke_editor(session, "game_query", {{"action", "inspect"}, {"object", "Crate1"}});
  REQUIRE(ok(crate));
  CHECK(text(crate).find("#18 Crate1 pos=9,9,9 body=dynamic") == 0);
  const auto ray = invoke_editor(session, "game_query", {{"action", "raycast"}, {"from", "0,5,-4"}, {"to", "0,-5,-4"}});
  CHECK(text(ray).find("Ground") != std::string::npos);
  CHECK_FALSE(ok(invoke_editor(session, "game_query", {{"action", "inspect"}, {"object", "Player"}, {"frames", 3}})));
  session.stop();
}

TEST_CASE("frame takes an object, and needs the editor window like the other view actions") {
  Game game;
  CHECK_FALSE(ok(invoke_editor(*game.session, "editor", {{"action", "frame"}, {"object", "Nope"}})));
  const auto windowless = invoke_editor(*game.session, "editor", {{"action", "frame"}, {"object", "Coin1"}});
  CHECK(text(windowless).find("requires an editor window") != std::string::npos);
  CHECK_FALSE(ok(invoke_editor(*game.session, "editor", {{"action", "undo"}, {"object", "Coin1"}})));
}
