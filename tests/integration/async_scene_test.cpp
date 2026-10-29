#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <stdexcept>

#include "app/engine.hpp"
#include "app/scene_loader.hpp"
#include "core/config.hpp"
#include "test_support.hpp"
#include "async_scene_support.hpp"

using namespace redgem;

namespace {
constexpr auto kWaitLimit = std::chrono::seconds(10);  // a dead worker fails instead of hanging the suite
constexpr int kLoadingFrames = 12;

std::unique_ptr<Engine> game(SceneLoader* loader = nullptr, bool fixed = false, const std::string& room = {}) {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.fixed_step = fixed;
  options.server_room = room;
  options.scene_loader = loader;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const auto result = engine.scripts().eval(code, "async_scene_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}
void frame(Engine& engine) { engine.frame(config::kFixedDeltaSeconds); }
void finish_loading(Engine& engine) {
  const auto deadline = std::chrono::steady_clock::now() + kWaitLimit;
  while (!engine.script_context().loading_scene.empty() && std::chrono::steady_clock::now() < deadline) {
    frame(engine);
    std::this_thread::yield();
  }
  REQUIRE(engine.script_context().loading_scene.empty());
}
}  // namespace

TEST_CASE("async scene reads freeze simulation but keep loading UI and other engines alive") {
  test::LogCapture logs;
  test::HeldSceneReader reader;
  auto first = game(&reader.loader, true, "LOAD01");
  auto second = game(&reader.loader, true, "FAST01");
  ruby(*first,
       "$saved = 7; $loading_draws = 0; $loading_delta = 0; "
       "Game.on_loading { |dt| $loading_draws += 1; $loading_delta += dt; UI.text('Please wait', x: 1, y: 1) }; "
       "Game.load_scene_async('level2')");
  CHECK(ruby(*first, "Game.loading?") == "true");
  frame(*first);
  reader.wait_started();
  const auto elapsed = first->script_context().time.elapsed;
  const auto other_elapsed = second->script_context().time.elapsed;
  for (int i = 0; i < kLoadingFrames; ++i) {
    frame(*first);
    frame(*second);
  }
  CHECK(first->script_context().time.elapsed == elapsed);
  CHECK(second->script_context().time.elapsed > other_elapsed);
  CHECK(ruby(*first, "$loading_draws") == std::to_string(kLoadingFrames));
  CHECK(std::stof(ruby(*first, "$loading_delta")) > 0.0f);
  CHECK_FALSE(first->script_context().hud.empty());
  CHECK(first->scene_name() == "main");
  CHECK_FALSE(logs.contains("background scene diagnostic"));
  reader.release();
  finish_loading(*first);
  CHECK(first->scene_name() == "level2");
  CHECK(ruby(*first, "$saved") == "7");
  CHECK(ruby(*first, "$level2_setup_in") == "level2");
  CHECK(ruby(*first, "Game.loading_error.inspect") == "nil");
  CHECK(logs.contains("background scene diagnostic"));
}

TEST_CASE("cancelled scene reads cannot replace a world after their engine is gone") {
  test::HeldSceneReader reader;
  auto engine = game(&reader.loader);
  ruby(*engine, "Game.load_scene_async('level2')");
  frame(*engine);
  reader.wait_started();
  CHECK(ruby(*engine, "Game.cancel_scene_load") == "true");
  frame(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading?") == "false");
  ruby(*engine, "Game.load_scene_async('level2')");
  frame(*engine);
  engine.reset();  // must not join the blocked parser or leave a raw Engine pointer in it
  reader.release();
}

TEST_CASE("a synchronous request supersedes a pending async read") {
  test::HeldSceneReader reader;
  auto engine = game(&reader.loader);
  ruby(*engine, "Game.load_scene_async('level2')");
  frame(*engine);
  reader.wait_started();
  ruby(*engine, "Game.load_scene('main')");
  frame(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading?") == "false");
  reader.release();
  frame(*engine);
  CHECK(engine->scene_name() == "main");
}

TEST_CASE("fixed-step asynchronous scene requests have reproducible activation frames") {
  auto engine = game(nullptr, true);
  ruby(*engine, "$loading_draws = 0; Game.on_loading { $loading_draws += 1 }; Game.load_scene_async('level2')");
  frame(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading?") == "true");
  frame(*engine);
  CHECK(engine->scene_name() == "level2");
  CHECK(ruby(*engine, "$loading_draws") == "1");
  CHECK(ruby(*engine, "Game.loading_scene.inspect") == "nil");
  ruby(*engine, "Game.load_scene_async('main')");
  frame(*engine);
  frame(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "$loading_draws") == "1");  // the old scene's callback was released
}

TEST_CASE("scene loader bounds uncollected work and reuses abandoned queue slots") {
  test::HeldSceneReader reader;
  std::vector<std::shared_ptr<SceneLoad>> slots;
  slots.push_back(reader.loader.submit(test::fixture("games/sandbox"), "level2"));
  reader.wait_started();
  for (std::size_t i = 1; i < SceneLoader::kOutstandingLimit; ++i)
    slots.push_back(reader.loader.submit(test::fixture("games/sandbox"), "level2"));
  CHECK_FALSE(reader.loader.submit(test::fixture("games/sandbox"), "level2"));
  slots.back().reset();
  CHECK(reader.loader.submit(test::fixture("games/sandbox"), "level2"));
  slots.clear();
  reader.release();
}

TEST_CASE("failed async scene reads preserve live object identities and expose the error") {
  SceneLoader reader([](const auto&, const auto&) { return LoadResult{false, "broken scene data", {}, {}}; });
  auto engine = game(&reader);
  const auto hero = engine->scene().find("Hero");
  REQUIRE(hero != kNoObject);
  const auto handle = engine->scene().handle(hero);
  ruby(*engine, "$kept = 42; Game.load_scene_async('level2')");
  frame(*engine);
  finish_loading(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(engine->scene().resolve(handle) == hero);
  CHECK(ruby(*engine, "$kept") == "42");
  CHECK(ruby(*engine, "Game.loading_error") == "broken scene data");
}

TEST_CASE("a newer async scene request discards the earlier result") {
  test::HeldSceneReader reader;
  auto engine = game(&reader.loader);
  ruby(*engine, "Game.load_scene_async('level2')");
  frame(*engine);
  reader.wait_started();
  ruby(*engine, "Game.load_scene_async('main')");
  frame(*engine);
  CHECK(ruby(*engine, "Game.loading_scene") == "main");
  reader.release();
  finish_loading(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "$level2_setup_in.inspect") == "nil");
}

TEST_CASE("loading callback failures are reported once and do not stop loading") {
  test::HeldSceneReader reader;
  auto engine = game(&reader.loader);
  ruby(*engine, "Game.on_loading { raise 'broken screen' }; Game.load_scene_async('level2')");
  frame(*engine);
  reader.wait_started();
  const auto before = engine->scripts().error_count();  // after the fixture's ordinary startup errors
  for (int i = 0; i < kLoadingFrames; ++i) frame(*engine);
  CHECK(engine->scripts().error_count() == before + 1);
  CHECK(ruby(*engine, "Game.loading?") == "true");
  reader.release();
  finish_loading(*engine);
  CHECK(engine->scene_name() == "level2");
}

TEST_CASE("a queued async request can be cancelled before its first frame") {
  auto engine = game(nullptr, true);
  CHECK(ruby(*engine, "Game.load_scene_async('level2'); Game.cancel_scene_load") == "true");
  frame(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading?") == "false");
}

TEST_CASE("the shared background scene reader activates a real scene file") {
  auto engine = game();
  ruby(*engine, "Game.load_scene_async('level2')");
  frame(*engine);
  finish_loading(*engine);
  CHECK(engine->scene_name() == "level2");
}

TEST_CASE("a throwing scene reader reports an error without killing its worker") {
  SceneLoader reader([](const std::filesystem::path&, const std::string&) -> LoadResult {
    throw std::runtime_error("reader exception");
  });
  auto engine = game(&reader);
  for (int attempt = 0; attempt < 2; ++attempt) {
    ruby(*engine, "Game.load_scene_async('level2')");
    frame(*engine);
    finish_loading(*engine);
    CHECK(engine->scene_name() == "main");
    CHECK(ruby(*engine, "Game.loading_error") == "reader exception");
  }
}

TEST_CASE("cancelling no load does not discard partial physics time") {
  auto engine = game();
  const auto half = config::kFixedDeltaSeconds * 0.5f;
  ruby(*engine, "Game.cancel_scene_load");
  engine->frame(half);
  const auto before = engine->script_context().time.elapsed;
  ruby(*engine, "Game.cancel_scene_load");
  engine->frame(half);
  CHECK(engine->script_context().time.elapsed > before);
}

TEST_CASE("a full async scene queue reports failure without replacing the world") {
  test::HeldSceneReader reader;
  std::vector<std::shared_ptr<SceneLoad>> slots;
  for (std::size_t i = 0; i < SceneLoader::kOutstandingLimit; ++i)
    slots.push_back(reader.loader.submit(test::fixture("games/sandbox"), "level2"));
  reader.wait_started();
  auto engine = game(&reader.loader);
  ruby(*engine, "Game.load_scene_async('level2')");
  frame(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading?") == "false");
  CHECK(ruby(*engine, "Game.loading_error.include?('queue is full')") == "true");
}

TEST_CASE("quitting from a loading screen abandons the incoming scene") {
  test::HeldSceneReader reader;
  auto engine = game(&reader.loader);
  ruby(*engine, "Game.on_loading { Game.quit }; Game.load_scene_async('level2')");
  frame(*engine);
  reader.wait_started();
  frame(*engine);
  CHECK(engine->finished());
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading?") == "false");
}

TEST_CASE("background scene diagnostics retain levels and have bounded memory") {
  test::LogCapture process;
  SceneLoader reader([](const std::filesystem::path&, const std::string&) {
    for (int i = 0; i < 1024; ++i) log::error(std::string(4096, 'x'));
    return LoadResult{false, "failed", {}, {}};
  });
  auto slot = reader.submit(test::fixture("games/sandbox"), "level2");
  std::optional<SceneLoad::Result> result;
  const auto deadline = std::chrono::steady_clock::now() + kWaitLimit;
  while (!result && std::chrono::steady_clock::now() < deadline) {
    result = slot->take();
    std::this_thread::yield();
  }
  REQUIRE(result);
  REQUIRE_FALSE(result->messages.empty());
  CHECK(result->messages.size() <= 129);
  CHECK(result->messages.front().first == log::Level::Error);
  CHECK(result->messages.front().second.size() <= 2051);
  CHECK(result->messages.back().second.find("dropped") != std::string::npos);
  CHECK_FALSE(process.contains("xxxx"));
}

TEST_CASE("missing async scene files fail later while unsafe names are rejected immediately") {
  auto engine = game();
  CHECK_FALSE(engine->scripts().eval("Game.load_scene_async('../outside')", "unsafe_scene.rb").ok);
  CHECK(ruby(*engine, "Game.loading?") == "false");
  ruby(*engine, "Game.load_scene_async('missing_scene')");
  CHECK(ruby(*engine, "Game.loading?") == "true");
  frame(*engine);
  finish_loading(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading_error.nil?") == "false");
}

TEST_CASE("fixed-step scene reads respect an embedder's reader and report empty failures") {
  SceneLoader reader([](const std::filesystem::path&, const std::string&) { return LoadResult{}; });
  auto engine = game(&reader, true);
  ruby(*engine, "Game.load_scene_async('level2')");
  frame(*engine);
  CHECK(ruby(*engine, "Game.loading?") == "true");
  frame(*engine);
  CHECK(engine->scene_name() == "main");
  CHECK(ruby(*engine, "Game.loading_error") == "scene read failed");
}

TEST_CASE("networked fixed-step games keep polling while a scene is read") {
  test::HeldSceneReader reader;
  auto engine = game(&reader.loader, true);
  std::string error;
  REQUIRE_MESSAGE(engine->network().host(0, error), error);
  ruby(*engine, "Game.load_scene_async('level2')");
  frame(*engine);
  reader.wait_started();
  frame(*engine);
  CHECK(ruby(*engine, "Game.loading?") == "true");
  ruby(*engine, "Game.cancel_scene_load");
  frame(*engine);
  CHECK(engine->scene_name() == "main");
}
