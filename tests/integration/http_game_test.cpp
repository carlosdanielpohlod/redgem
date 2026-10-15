// HTTP in a running game: answers reach the scripts on the game's own thread, and a game that
// ends (or a Play in the editor that is stopped) takes its requests with it.

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <thread>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "editor/editor_session.hpp"
#include "http_test_server.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;
using redgem::test::http_answer;
using redgem::test::HttpTestServer;

namespace {

constexpr std::chrono::milliseconds kFramePause{2};
constexpr std::chrono::seconds kLongestWait{10};

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

bool eventually(const std::function<bool()>& done, const std::function<void()>& meanwhile = [] {}) {
  const auto give_up = std::chrono::steady_clock::now() + kLongestWait;
  while (!done() && std::chrono::steady_clock::now() < give_up) {
    meanwhile();
    std::this_thread::sleep_for(kFramePause);
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
  REQUIRE(eventually([&game] { return ruby(*game, "$got.nil?") == "false"; },
                     [&game] { game->frame(config::kFixedDeltaSeconds); }));
  CHECK(ruby(*game, "$got.inspect") == "[true, 200, 3, [\"ana\"]]");
  const std::string seen = server.last_request();
  CHECK(seen.find("\r\nContent-Type: application/json\r\n") != std::string::npos);
  CHECK(seen.substr(seen.find("\r\n\r\n") + 4) == "{\"player\":\"ana\",\"points\":120}");
  std::filesystem::remove(save_file());
}

TEST_CASE("a game that ends gives its requests up: no block of it is ever called") {
  test::LogCapture quiet;
  HttpTestServer server(HttpTestServer::Kind::Plain, "");
  server.keep_quiet();
  {
    auto game = sandbox_game();
    ruby(*game, "HTTP.get('" + server.url("/slow") + "') { raise 'never' }");
    REQUIRE(eventually([&server] { return server.requests() == 1; }));
  }
  CHECK(eventually([&server] { return server.saw_hang_up(); }));
  std::filesystem::remove(save_file());
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
  REQUIRE(eventually([&server] { return server.requests() == 1; }));
  session->stop();
  CHECK(eventually([&server] { return server.saw_hang_up(); }));
}
