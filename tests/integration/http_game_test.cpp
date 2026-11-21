// HTTP in a running game: answers reach the scripts on the game's own thread, and a game that
// ends (or a Play in the editor that is stopped) takes its requests with it.

#include <doctest/doctest.h>

#include <filesystem>
#include <functional>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "editor/editor_session.hpp"
#include "http_test_server.hpp"
#include "storage/save_file.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;
using redgem::test::http_answer;
using redgem::test::HttpTestServer;

namespace {

// A frame waits this long for an answer at most before it plays; there are that many frames at
// most (a bound for an answer that never comes, never a measure).
constexpr float kAnswerLook = 0.002f;
constexpr int kMostFrames = 5000;

std::filesystem::path save_file() { return std::filesystem::temp_directory_path() / "redgem-http-game-test.json"; }

std::unique_ptr<Engine> sandbox_game() {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  options.save_file = save_file();
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

// Plays frames of `game` until `done`, each once an answer waits for it (or a moment went by).
bool frames_until(Engine& game, const std::function<bool()>& done) {
  for (int frame = 0; frame < kMostFrames && !done(); ++frame) {
    game.http().wait(kAnswerLook);
    game.frame(config::kFixedDeltaSeconds);
  }
  return done();
}

}  // namespace

TEST_CASE("a script's request is answered in its block, on a frame of the game, with JSON both ways") {
  test::LogCapture quiet;
  HttpTestServer server(HttpTestServer::Kind::Plain, http_answer("200 OK", "{\"rank\":3,\"top\":[\"ana\"]}"));
  auto game = sandbox_game();
  ruby(*game,
       "$got = nil; HTTP.post('" + server.url("/scores") +
           "', json: { player: 'ana', points: 120 }) { |r| $got = [r.ok?, r.status, r.json[:rank], r.json[:top]] }");
  CHECK(ruby(*game, "$got.inspect") == "nil");  // the frame never waits
  REQUIRE(frames_until(*game, [&game] { return ruby(*game, "$got.nil?") == "false"; }));
  CHECK(ruby(*game, "$got.inspect") == "[true, 200, 3, [\"ana\"]]");
  const std::string seen = server.last_request();
  CHECK(seen.find("\r\nContent-Type: application/json\r\n") != std::string::npos);
  CHECK(seen.substr(seen.find("\r\n\r\n") + 4) == "{\"player\":\"ana\",\"points\":120}");
  remove_save(save_file());
}

TEST_CASE("a script's answer says where it came from once redirects were followed") {
  test::LogCapture quiet;
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.answer_in_turn(
      {http_answer("308 Permanent Redirect", "", "Location: /v2/news\r\n"), http_answer("200 OK", "{\"news\":1}")});
  auto game = sandbox_game();
  ruby(*game, "$got = nil; HTTP.get('" + server.url("/news") + "') { |r| $got = [r.ok?, r.url, r.json[:news]] }");
  REQUIRE(frames_until(*game, [&game] { return ruby(*game, "$got.nil?") == "false"; }));
  CHECK(ruby(*game, "$got.inspect") == "[true, \"" + server.url("/v2/news") + "\", 1]");
  remove_save(save_file());
}

TEST_CASE("a game that ends gives its requests up: no block of it is ever called") {
  test::LogCapture quiet;
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.keep_quiet();
  {
    auto game = sandbox_game();
    ruby(*game, "HTTP.get('" + server.url("/slow") + "') { raise 'never' }");
    REQUIRE(server.wait_for_requests(1));
  }
  CHECK(server.wait_for_hang_up());
  remove_save(save_file());
}

TEST_CASE("stopping a game played in the editor gives its requests up") {
  test::LogCapture quiet;
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.keep_quiet();
  auto opened = EditorSession::open(fixture("games/sandbox"));
  REQUIRE(std::holds_alternative<std::unique_ptr<EditorSession>>(opened));
  auto& session = std::get<std::unique_ptr<EditorSession>>(opened);
  ScriptedInputSource input;
  std::string error;
  REQUIRE_MESSAGE(session->play({input}, error), error);
  REQUIRE(session->runtime()->scripts().eval("HTTP.get('" + server.url("/") + "') { raise 'never' }", "t.rb").ok);
  REQUIRE(server.wait_for_requests(1));
  session->stop();
  CHECK(server.wait_for_hang_up());
}
