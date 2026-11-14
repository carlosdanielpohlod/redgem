// The editor's opt-in Ruby evaluation (project/ruby_eval.hpp). Built only with REDGEM_RUBY_EVAL,
// as the code it tests is; tests/ships_no_ruby_eval.sh checks that release programs have none of it.
#include <doctest/doctest.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <variant>

#include "core/files.hpp"
#include "editor/editor_session.hpp"
#include "project/cli.hpp"
#include "project/editor_bridge.hpp"
#include "project/editor_live_tools.hpp"
#include "project/mcp_tools.hpp"
#include "project/ruby_eval.hpp"
#include "script/ruby_vm.hpp"

using namespace redgem;
using namespace redgem::tools;
using mcp::Json;
namespace fs = std::filesystem;

namespace {
constexpr const char* kToken = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
constexpr std::chrono::milliseconds kShortBudget{50};
// Far longer than kMostEvalBudget: an evaluation must not run on it.
constexpr std::chrono::milliseconds kLongBudget{60 * 1000};
// What an evaluation that is stopped may take at most, with the budget's grace to unwind.
constexpr std::chrono::seconds kStoppedWithin{4};
// More than a tool's text is cut to (8 KiB) with its notice.
constexpr std::size_t kMostAnswerBytes = 8400;

bool failed(const Json& result) { return result.at("isError").get<bool>(); }
std::string text_of(const Json& result) { return result["content"][0]["text"]; }
bool says(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
Json call(const std::string& token, const std::string& code) { return {{"token", token}, {"code", code}}; }

// A copy of the platformer, open in an editor session.
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
  ~Game() {
    session.reset();  // close the recovery lease before removing its directory on Windows
    fs::remove_all(root);
  }
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;
  fs::path root = fs::temp_directory_path() / "redgem-ruby-eval-test";
  fs::path path = root / "game";
  std::unique_ptr<EditorSession> session;
};

// A call over the editor's real loopback bridge, answered as an editor started with `gate`
// (null: without --ruby-eval) answers it.
Json over_bridge(Game& game, const RubyEvalGate* gate, const Json& args) {
  EditorBridge server(game.path, game.root / "private");
  const mcp::Options options{game.path, false, {}, server.connection_file()};
  auto future = std::async(std::launch::async, [&] { return mcp::invoke(options, kRubyEvalTool, args); });
  while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
    server.tick([&](const std::string& tool, const Json& values) {
      if (is_ruby_eval_tool(tool)) return answer_ruby_eval(gate, *game.session, values);
      return invoke_editor(*game.session, tool, values);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return future.get();
}

const Json* listed(const Json& tools, const std::string& name) {
  for (const auto& tool : tools)
    if (tool["name"] == name) return &tool;
  return nullptr;
}
}  // namespace

TEST_CASE("an evaluation returns its result inspected") {
  RubyVm vm;
  const EvalReply reply = evaluate_ruby(vm, "[1, :two, 'three', nil, 1 + 2]");
  CHECK(reply.ok);
  CHECK(reply.text == "[1, :two, \"three\", nil, 3]");
  CHECK(evaluate_ruby(vm, "$kept = 41").ok);  // the top level is the game's own
  CHECK(evaluate_ruby(vm, "$kept + 1").text == "42");
}

TEST_CASE("an evaluation that raises reports the exception with its backtrace") {
  RubyVm vm;
  const EvalReply reply = evaluate_ruby(vm, "def explode\n  raise ArgumentError, 'kaboom'\nend\nexplode");
  CHECK_FALSE(reply.ok);
  CHECK(says(reply.text, "ArgumentError"));
  CHECK(says(reply.text, "kaboom"));
  CHECK(says(reply.text, "(game_eval)"));
  CHECK(evaluate_ruby(vm, "1").ok);  // the interpreter goes on
}

TEST_CASE("an evaluation that never returns is stopped by the script budget, which is given back") {
  RubyVm vm;
  vm.set_budget(kShortBudget);
  const EvalReply stopped = evaluate_ruby(vm, "loop {}");
  CHECK_FALSE(stopped.ok);
  CHECK(says(stopped.text, "ScriptTimeout"));
  CHECK(vm.budget() == kShortBudget);
  // Without a budget, or with one longer than the bridge waits, it runs on kMostEvalBudget.
  for (const auto own : {std::chrono::milliseconds(0), kLongBudget}) {
    vm.set_budget(own);
    const auto started = std::chrono::steady_clock::now();
    CHECK(says(evaluate_ruby(vm, "loop {}").text, "ScriptTimeout"));
    CHECK(std::chrono::steady_clock::now() - started < kStoppedWithin);
    CHECK(vm.budget() == own);
  }
  vm.set_budget(std::chrono::milliseconds(0));
}

TEST_CASE("an evaluation's answer stops at the tools' size limit") {
  RubyVm vm;
  const RubyEvalGate gate(kToken);
  const Json answer = gate.answer(&vm, call(kToken, "'x' * 100_000"));
  CHECK_FALSE(failed(answer));
  CHECK(text_of(answer).size() < kMostAnswerBytes);
  CHECK(says(text_of(answer), "output cut at 8 KiB"));
}

TEST_CASE("the gate refuses a wrong or missing token, too much code and a game that does not play") {
  RubyVm vm;
  const RubyEvalGate gate(kToken);
  for (const Json& args : {call("wrong", "$ran = true"), Json{{"code", "$ran = true"}}, Json::object()}) {
    const Json refused = gate.answer(&vm, args);
    CHECK(failed(refused));
    CHECK(says(text_of(refused), "token"));
  }
  CHECK(evaluate_ruby(vm, "$ran").text == "nil");  // nothing ran
  const Json too_long = gate.answer(&vm, call(kToken, std::string(kMostEvalCodeBytes + 1, ' ')));
  CHECK(failed(too_long));
  CHECK(says(text_of(too_long), "longer than"));
  CHECK(failed(gate.answer(&vm, call(kToken, ""))));
  for (const Json& no_code : {Json{{"token", kToken}}, Json{{"token", kToken}, {"code", Json::array()}}}) {
    CHECK(says(text_of(gate.answer(&vm, no_code)), "needs code"));
  }
  CHECK(says(text_of(gate.answer(nullptr, call(kToken, "1"))), "not playing"));
  const Json answered = gate.answer(&vm, call(kToken, "6 * 7"));
  CHECK_FALSE(failed(answered));
  CHECK(text_of(answered) == "42");
}

TEST_CASE("opening the gate prints a fresh random token") {
  std::ostringstream first;
  std::ostringstream second;
  const auto gate = open_ruby_eval(first);
  const auto other = open_ruby_eval(second);
  CHECK(first.str() == "ruby eval token: " + gate->token() + "\n");
  CHECK(gate->token().size() == std::strlen(kToken));
  CHECK(gate->token() != other->token());
}

TEST_CASE("the evaluation is opt-in: --ruby-eval turns it on, with the bridge it is asked through") {
  CHECK_FALSE(parse_cli({"games/demo"}).options.ruby_eval);
  CHECK_FALSE(parse_cli({"games/demo", "--editor-bridge"}).options.ruby_eval);
  const auto opened = parse_cli({"games/demo", "--ruby-eval"});
  REQUIRE(opened.ok);
  CHECK(opened.options.ruby_eval);
  CHECK(opened.options.editor_bridge);
  CHECK_FALSE(parse_cli({"games/demo", "--ruby-eval", "--play"}).ok);
  CHECK_FALSE(parse_cli({"build", "games/demo", "out", "--ruby-eval"}).ok);
}

TEST_CASE("game_eval is listed only for a connected editor, as a tool that changes the game") {
  mcp::Options options;
  CHECK(listed(mcp::catalog(options), kRubyEvalTool) == nullptr);
  options.editor_connection = "connection.json";
  const Json tools = mcp::catalog(options);
  const Json* tool = listed(tools, kRubyEvalTool);
  REQUIRE(tool != nullptr);
  CHECK((*tool)["annotations"]["destructiveHint"] == true);
  CHECK((*tool)["inputSchema"]["properties"]["code"]["maxLength"] == kMostEvalCodeBytes);
  options.read_only = true;
  CHECK(listed(mcp::catalog(options), kRubyEvalTool) == nullptr);
}

TEST_CASE("with no editor running, game_eval says how to start one that has it") {
  const fs::path root = fs::temp_directory_path() / "redgem-ruby-eval-no-editor";
  fs::create_directories(root);
  mcp::Options options;
  options.game = REDGEM_PLATFORMER_DIR;
  options.editor_connection = mcp::kDiscoverEditor;
  options.bridge_root = root;
  const Json answer = mcp::invoke(options, kRubyEvalTool, call(kToken, "1"));
  CHECK(failed(answer));
  CHECK(says(text_of(answer), "--ruby-eval"));
  fs::remove_all(root);
}

TEST_CASE("over the bridge, game_eval runs in the playing game with the token, and only then") {
  Game game;
  const RubyEvalGate gate(kToken);
  const Json editing = over_bridge(game, &gate, call(kToken, "1 + 1"));
  CHECK(says(text_of(editing), "not playing"));
  ScriptedInputSource input;
  std::string error;
  REQUIRE(game.session->play({input}, error));
  const Json answered = over_bridge(game, &gate, call(kToken, "Scene.find('Player').name"));
  CHECK_FALSE(failed(answered));
  CHECK(text_of(answered) == "\"Player\"");
  CHECK(failed(over_bridge(game, &gate, call("wrong", "1"))));
  const Json off = over_bridge(game, nullptr, call(kToken, "1"));
  CHECK(failed(off));
  CHECK(says(text_of(off), "--ruby-eval"));
  game.session->stop();
}
