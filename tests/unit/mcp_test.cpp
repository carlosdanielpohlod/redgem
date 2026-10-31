#include <doctest/doctest.h>

#include <filesystem>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "core/event_journal.hpp"
#include "project/mcp_protocol.hpp"
#include "project/mcp_tools.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::tools::mcp;
namespace {
Json request(const std::string& method, Json params = Json::object(), Json id = 1) {
  return {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
}
Json modern(const std::string& method, Json params = Json::object()) {
  params["_meta"] = {{"io.modelcontextprotocol/protocolVersion", kModernVersion},
                     {"io.modelcontextprotocol/clientCapabilities", Json::object()}};
  return request(method, params);
}
Session server(const Options& options = {}) {
  return Session(catalog(options),
                 [options](const std::string& name, const Json& args) { return invoke(options, name, args); });
}
Json call(const std::string& name, Json args = Json::object()) {
  return modern("tools/call", {{"name", name}, {"arguments", args}});
}
bool contains(const Json& value, const std::string& part) { return value.dump().find(part) != std::string::npos; }
}  // namespace

TEST_CASE("MCP legacy initialization gates calls until the initialized notification") {
  auto session = server();
  CHECK(session.receive(request("ping"))->contains("result"));
  CHECK(session.receive(request("tools/list"))->contains("error"));
  const auto init =
      session.receive(request("initialize", {{"protocolVersion", "future-version"}, {"capabilities", Json::object()}}));
  CHECK((*init)["result"]["protocolVersion"] == kLegacyVersion);
  CHECK(session.receive(request("tools/list"))->contains("error"));
  CHECK_FALSE(session.receive({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}));
  const auto listed = session.receive(request("tools/list"));
  CHECK((*listed)["result"]["tools"].size() == 10);
  CHECK_FALSE((*listed)["result"].contains("resultType"));
}

TEST_CASE("MCP legacy initialization answers an older supported version with that version") {
  for (const char* version : {"2025-06-18", "2025-03-26", "2025-11-25"}) {
    auto session = server();
    const auto init =
        session.receive(request("initialize", {{"protocolVersion", version}, {"capabilities", Json::object()}}));
    CHECK((*init)["result"]["protocolVersion"] == version);
  }
}

TEST_CASE("MCP modern discovery and calls carry metadata independently of prior requests") {
  auto session = server();
  const auto discovery = session.receive(modern("server/discover"));
  CHECK((*discovery)["result"]["supportedVersions"] == Json::array({kModernVersion}));
  CHECK((*discovery)["result"]["resultType"] == "complete");
  CHECK((*discovery)["result"]["cacheScope"] == "private");
  CHECK(session.receive(call("api", {{"class", "Transform"}}))->contains("result"));
  CHECK(session.receive(request("tools/list"))->contains("error"));
  Json bad = modern("tools/list");
  bad["params"]["_meta"]["io.modelcontextprotocol/protocolVersion"] = "unknown";
  CHECK((*session.receive(bad))["error"]["code"] == -32022);
  CHECK((*session.receive(bad))["error"]["data"]["supported"] == Json::array({kModernVersion}));
}

TEST_CASE("MCP rejects malformed metadata and envelopes and preserves request IDs") {
  auto session = server();
  CHECK((*session.receive(Json::array()))["error"]["code"] == -32600);
  CHECK((*session.receive(request("ping", nullptr)))["error"]["code"] == -32600);
  CHECK((*session.receive(request("ping", Json::object(), nullptr)))["error"]["code"] == -32600);
  Json bad = modern("ping");
  bad["params"]["_meta"].erase("io.modelcontextprotocol/clientCapabilities");
  CHECK((*session.receive(bad))["error"]["code"] == -32602);
  Json asked = modern("missing");
  asked["id"] = "opaque-request";
  CHECK((*session.receive(asked))["id"] == "opaque-request");
  CHECK((*session.receive(asked))["error"]["code"] == -32601);
  CHECK_FALSE(session.receive({{"jsonrpc", "2.0"}, {"method", "tools/call"}}));
}

TEST_CASE("MCP read-only and allow lists restrict discovery and invocation together") {
  Options options;
  options.read_only = true;
  options.allow = {"scene_edit", "api"};
  auto session = server(options);
  CHECK((*session.receive(modern("tools/list")))["result"]["tools"].size() == 1);
  CHECK((*session.receive(
            call("scene_edit", {{"action", "remove"}, {"object", "Player"}, {"confirm", true}})))["error"]["code"] ==
        -32602);
  CHECK_FALSE((*session.receive(call("api")))["result"]["isError"].get<bool>());
}

namespace {
Json result_of(Session& session, const std::string& name, const Json& args) {
  return (*session.receive(call(name, args)))["result"];
}
std::string text_of(const Json& result) { return result["content"][0]["text"].get<std::string>(); }
}  // namespace

TEST_CASE("MCP schemas reject unknown, wrong-type and out-of-range tool arguments as tool errors") {
  auto session = server();
  for (const Json& args : {Json{{"frames", -1}}, Json{{"frames", 601}}, Json{{"frames", 1.5}}, Json{{"frames", true}},
                           Json{{"game", "/elsewhere"}}, Json{{"seed", 2147483648ULL}}}) {
    CHECK(result_of(session, "game_run", args)["isError"] == true);
  }
  CHECK(result_of(session, "scene_get", Json::object())["isError"] == true);
  CHECK(result_of(session, "api", {{"class", std::string("nul\0name", 8)}})["isError"] == true);
  CHECK(result_of(session, "events", {{"since", -1}})["isError"] == true);
}

TEST_CASE("MCP argument errors name the argument and what it accepts") {
  auto session = server();
  CHECK(text_of(result_of(session, "game_run", {{"frames", 601}})) == "frames: must be at most 600, not 601");
  CHECK(text_of(result_of(session, "scene_list", {{"limit", -1}})) == "limit: must be at least 0, not -1");
  CHECK(text_of(result_of(session, "scene_list", {{"limit", "5"}})) == "limit: must be a whole number");
  CHECK(text_of(result_of(session, "scene_edit", {{"action", "sett"}, {"object", "Coin1"}})) ==
        "action: must be one of: set, add, remove");
  CHECK(text_of(result_of(session, "scene_get", Json::object())) == "missing argument: object");
  CHECK(contains(result_of(session, "game_run", {{"game", "/elsewhere"}}), "unknown argument: game (accepted: "));
  const Json long_word{{"action", "set"}, {"object", "Coin1"}, {"assignments", {std::string(4097, 'x')}}};
  CHECK(text_of(result_of(session, "scene_edit", long_word)) ==
        "assignments: item 1: is 4097 bytes; the limit is 4096");
}

TEST_CASE("MCP unknown tools stay protocol errors") {
  auto session = server();
  CHECK((*session.receive(call("no_such_tool")))["error"]["code"] == -32602);
}

TEST_CASE("MCP confines input and comparison files and requires explicit deletion") {
  const Options options{REDGEM_PLATFORMER_DIR, false, {}};
  CHECK_THROWS_AS(invoke(options, "scene_diff", {{"against", "../outside.glb"}}), std::invalid_argument);
  CHECK_THROWS_AS(invoke(options, "game_run", {{"input", "/dev/zero"}}), std::invalid_argument);
  CHECK_THROWS_AS(invoke(options, "scene_edit", {{"action", "remove"}, {"object", "Player"}}), std::invalid_argument);
  CHECK_THROWS_AS(invoke(options, "scene_edit", {{"action", "set"}, {"object", "Player"}, {"from", "Coin1"}}),
                  std::invalid_argument);
}

TEST_CASE("MCP exposes native read-only scene and API commands") {
  const Options options{REDGEM_PLATFORMER_DIR, true, {}};
  CHECK(contains(invoke(options, "scene_list", {{"limit", 2}}), "27 objects"));
  CHECK_FALSE(contains(invoke(options, "scene_list", {{"limit", 0}}), "more not shown"));
  CHECK(contains(invoke(options, "scene_get", {{"object", "Player"}}), "Player"));
  CHECK(contains(invoke(options, "scene_diff", {{"against", "scene.glb"}}), "no object changes"));
  CHECK(contains(invoke(options, "api", {{"class", "Transform"}}), "position"));
}

namespace {
// The cursor a page of events says to pass next: its first line is "cursor=N ...".
constexpr const char* kCursorPrefix = "cursor=";
std::uint64_t page_cursor(const std::string& text) {
  const std::size_t start = std::string(kCursorPrefix).size();
  return std::stoull(text.substr(start, text.find_first_of(" \n") - start));
}
}  // namespace

TEST_CASE("MCP event pages lead with their cursor and bound individual text") {
  const test::LogCapture capture;
  const auto cursor = log::event_cursor();
  log::warning(std::string(3000, 'x'));
  const std::string text = text_of(invoke({}, "events", {{"since", cursor}, {"limit", 1}}));
  CHECK(text.rfind("cursor=", 0) == 0);
  CHECK(page_cursor(text) > cursor);
  CHECK(text.find("warning " + std::string(256, 'x') + "...\n") != std::string::npos);
  CHECK(text.find(std::string(257, 'x')) == std::string::npos);
}

TEST_CASE("MCP event pages keep their cursor when the most events of the longest text fill a page") {
  const test::LogCapture capture;
  const auto cursor = log::event_cursor();
  for (int i = 0; i < 32; ++i) log::warning(std::string(3000, 'y'));
  const Json reply = invoke({}, "events", {{"since", cursor}, {"limit", 32}});
  CHECK(page_cursor(text_of(reply)) > cursor);
  CHECK_FALSE(reply.contains("structuredContent"));
}

TEST_CASE("MCP events read as one compact line each, with paths inside the game and the next cursor") {
  const test::LogCapture capture;
  const Options options{REDGEM_PLATFORMER_DIR, false, {}};
  const auto cursor = log::event_cursor();
  const std::string game = std::filesystem::path(REDGEM_PLATFORMER_DIR).generic_string();
  log::error("Coin#update on 'Coin1': boom (RuntimeError)\n    from " + game + "/scripts/coin.rb:12:in update");
  const std::string text = text_of(invoke(options, "events", {{"since", cursor}}));
  CHECK(text.find("error scripts/coin.rb:12: Coin#update on 'Coin1': boom (RuntimeError)") != std::string::npos);
  CHECK(text.find(game) == std::string::npos);
  const auto next = page_cursor(text);
  CHECK(text_of(invoke(options, "events", {{"since", next}})).find("no new events") != std::string::npos);
}

TEST_CASE("MCP framing recovers after malformed, oversized and deeply nested messages") {
  auto session = server();
  const std::string too_deep = std::string(100, '[') + "0" + std::string(100, ']');
  std::istringstream input("{bad\n" + std::string(kMaxRequestBytes + 1, 'x') + "\n" + too_deep + "\n" +
                           modern("ping").dump() + "\n");
  std::vector<Json> replies;
  serve(input, [&](const std::string& reply) { replies.push_back(Json::parse(reply)); }, session);
  REQUIRE(replies.size() == 4);
  CHECK(replies[0]["error"]["code"] == -32700);
  CHECK(replies[1]["error"]["code"] == -32600);
  CHECK(replies[2]["error"]["code"] == -32700);
  CHECK(replies[3]["result"]["resultType"] == "complete");
}

TEST_CASE("MCP tool failures are results and long UTF-8 output is bounded") {
  const Json failed = tool_result("cannot open scene", true);
  CHECK(failed["isError"] == true);
  const std::string text = std::string(8191, 'x') + "é";
  const Json bounded = tool_result(text);
  CHECK(contains(bounded, "output cut at 8 KiB"));
  CHECK_FALSE(contains(bounded, "é"));
  CHECK(bounded.dump().size() < 8400);
}
