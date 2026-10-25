// Scripts that never return: each call into them has a budget of time (game.json's
// script_budget_ms), past which it is stopped, its behaviour disabled, and the game goes on.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>

#if !defined(_WIN32)
#include <pthread.h>
#endif

#include "app/engine.hpp"
#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "script/ruby_vm.hpp"
#include "script/script_budget.hpp"
#include "script/script_config.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

using Clock = std::chrono::steady_clock;

// The longest a test of the guard may take: one whose script the guard failed to stop ends the
// test program here, failing, instead of spinning for as long as nobody stops it.
constexpr std::chrono::seconds kTestDeadline{30};

// Ends the program unless it is let go of before the deadline.
class TestDeadline {
 public:
  explicit TestDeadline(const char* what) : what_(what), watch_([this] { watch(); }) {}
  ~TestDeadline() {
    {
      const std::lock_guard<std::mutex> hold(lock_);
      done_ = true;
    }
    wake_.notify_all();
    watch_.join();
  }
  TestDeadline(const TestDeadline&) = delete;
  TestDeadline& operator=(const TestDeadline&) = delete;

 private:
  void watch() {
    std::unique_lock<std::mutex> hold(lock_);
    if (wake_.wait_for(hold, kTestDeadline, [this] { return done_; })) return;
    std::fprintf(stderr, "%s: the script was never stopped; ending the tests\n", what_);
    std::_Exit(EXIT_FAILURE);
  }

  const char* what_;
  std::mutex lock_;
  std::condition_variable wake_;
  bool done_ = false;
  std::thread watch_;  // (last: it starts once the rest is ready)
};

constexpr std::chrono::milliseconds kTestBudget{100};
constexpr int kTestBudgetMs = 100;  // what the test games' game.json says
// Far more than a stopped call takes (its budget, and a few microseconds past it), far less than
// a call that was never stopped would.
constexpr std::chrono::seconds kStoppedWell{5};
constexpr int kFramesAfter = 3;

// Behaviours that never return from their update, and one that just counts its frames.
const std::map<std::string, std::string> kScripts = {
    {"looper.rb", "class Looper < Behaviour\n  def update(_dt) = loop {}\nend\n"},
    {"spinner.rb", "class Spinner < Behaviour\n  def update(_dt)\n    while true; end\n  end\nend\n"},
    {"diver.rb", "class Diver < Behaviour\n  def update(_dt) = dive(0)\n  def dive(depth) = dive(depth + 1)\nend\n"},
    {"block_diver.rb",
     "class BlockDiver < Behaviour\n  def update(_dt) = dive\n  def dive = [1].each { send(:dive) }\nend\n"},
    // Rescuing every exception, even the budget's, still does not keep it running.
    {"stubborn.rb",
     "class Stubborn < Behaviour\n  def update(_dt)\n    loop do\n      begin\n        loop {}\n"
     "      rescue Exception\n        $rescued = true\n      ensure\n        $ensured = true\n      end\n"
     "    end\n  end\nend\n"},
    {"starter.rb",
     "class Starter < Behaviour\n  def start = loop {}\n  def update(_dt) = $starter_updated = true\nend\n"},
    {"counter.rb", "class Counter < Behaviour\n  def update(_dt) = $counted = ($counted || 0) + 1\nend\n"},
    // A stopped call's ensure runs, on a short grace: it may put things back, even spawn.
    {"resetter.rb",
     "class Resetter < Behaviour\n  def update(_dt)\n    $state = :busy\n    loop {}\n  ensure\n"
     "    $state = :idle\n  end\nend\n"},
    {"endless_ensure.rb",
     "class EndlessEnsure < Behaviour\n  def update(_dt)\n    loop {}\n  ensure\n    loop {}\n  end\nend\n"},
    {"spawner.rb",
     "class Spawner < Behaviour\n  def update(_dt)\n    loop {}\n  ensure\n    Scene.spawn('innocent.rb')\n  "
     "end\nend\n"},
    {"innocent.rb", "class Innocent < Behaviour\n  def start = $innocents = ($innocents || 0) + 1\nend\n"},
    // Calls the engine makes from inside a call are not that call's time: two slow objects made
    // from one update, each nearly a budget long, leave it well within its own.
    {"slow.rb",
     "class Slow < Behaviour\n  def initialize(*arguments)\n    super\n    started = Time.now\n"
     "    nil while Time.now - started < 0.07\n  end\nend\n"},
    // A loop that only ever spawns: the engine's work it does is forgiven only up to a point.
    {"breeder.rb", "class Breeder < Behaviour\n  def update(_dt) = loop { Scene.spawn('innocent.rb') }\nend\n"},
    {"patient.rb",
     "class Patient < Behaviour\n  def update(_dt)\n    return if $patient_done\n"
     "    2.times { Scene.spawn('slow.rb') }\n    $patient_done = true\n  end\nend\n"},
};

// A game of its own in a temporary folder, with the scripts above and a budget of kTestBudgetMs.
std::filesystem::path budget_game(const std::string& name, const std::string& settings) {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / ("redgem-budget-" + name);
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / config::kScriptsDirName);
  std::ofstream(game / "game.json") << settings;
  for (const auto& [file, code] : kScripts) std::ofstream(game / config::kScriptsDirName / file) << code;
  return game;
}

std::string budget_settings() { return "{\"script_budget_ms\": " + std::to_string(kTestBudgetMs) + "}"; }

SceneDesc scene_of(std::initializer_list<const char*> scripts) {
  SceneDesc desc;
  for (const char* script : scripts) {
    NodeDesc node;
    node.name = script;
    node.props = {{"script", std::string(script)}};
    desc.nodes.push_back(node);
  }
  return desc;
}

std::unique_ptr<Engine> started(const std::filesystem::path& game, const SceneDesc& scene) {
  EngineOptions options;
  options.game_dir = game;
  options.fixed_step = true;
  options.scene = &scene;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

// Runs frames; how long they took.
Clock::duration frames(Engine& engine, int count) {
  const Clock::time_point start = Clock::now();
  for (int i = 0; i < count; ++i) engine.frame(config::kFixedDeltaSeconds);
  return Clock::now() - start;
}

// Whether a script stuck in `script` is stopped on its first frame, reported as such, and the game
// goes on with its other behaviours.
void check_stopped(const char* script, const char* reported_as, const char* error) {
  const TestDeadline deadline(script);
  const std::filesystem::path game = budget_game(script, budget_settings());
  const SceneDesc scene = scene_of({script, "counter.rb"});
  test::LogCapture capture;
  std::unique_ptr<Engine> engine = started(game, scene);
  CHECK(frames(*engine, kFramesAfter) < kStoppedWell);
  CHECK(engine->scripts().error_count() == 1);
  CHECK(capture.contains(std::string(reported_as) + " on '" + script + "' (behaviour disabled)"));
  CHECK(capture.contains(error));
  CHECK(ruby(*engine, "$counted") == std::to_string(kFramesAfter));
  engine.reset();
  std::filesystem::remove_all(game);
}

}  // namespace

TEST_CASE("a behaviour stuck in loop {} is stopped past its budget, disabled, and the game goes on") {
  check_stopped("looper.rb", "Looper#update", "ScriptTimeout");
  check_stopped("looper.rb", "Looper#update", "ms a call into the scripts may take (script_budget_ms in game.json)");
}

TEST_CASE("a behaviour stuck in while true is stopped the same way") {
  check_stopped("spinner.rb", "Spinner#update", "ScriptTimeout");
}

TEST_CASE("a behaviour that rescues every exception is stopped all the same") {
  check_stopped("stubborn.rb", "Stubborn#update", "ScriptTimeout");
}

TEST_CASE("a recursion too deep ends with SystemStackError, not a crash") {
  check_stopped("diver.rb", "Diver#update", "SystemStackError");
  // (Through a block and a method called by name: the engine's stack is never what runs out.)
  check_stopped("block_diver.rb", "BlockDiver#update", "SystemStackError");
}

TEST_CASE("a start that never returns is stopped, and its behaviour never updated") {
  const TestDeadline deadline("start");
  const std::filesystem::path game = budget_game("start", budget_settings());
  const SceneDesc scene = scene_of({"starter.rb", "counter.rb"});
  test::LogCapture capture;
  std::unique_ptr<Engine> engine = started(game, scene);
  frames(*engine, kFramesAfter);
  CHECK(capture.contains("Starter#start on 'starter.rb' (behaviour disabled)"));
  CHECK(ruby(*engine, "$starter_updated.inspect") == "nil");
  CHECK(ruby(*engine, "$counted") == std::to_string(kFramesAfter));
  engine.reset();
  std::filesystem::remove_all(game);
}

TEST_CASE("code the engine runs (a scene script, a console line) is stopped the same way") {
  const TestDeadline deadline("eval");
  const std::filesystem::path game = budget_game("eval", budget_settings());
  const SceneDesc scene = scene_of({"counter.rb"});
  std::unique_ptr<Engine> engine = started(game, scene);
  const Clock::time_point start = Clock::now();
  const RubyResult stuck = engine->scripts().eval("x = 0\nloop { x += 1 }", "stuck.rb");
  CHECK(Clock::now() - start < kStoppedWell);
  CHECK_FALSE(stuck.ok);
  CHECK(stuck.error.find("ScriptTimeout") != std::string::npos);
  // The next call has a budget of its own.
  CHECK(ruby(*engine, "40 + 2") == "42");
  engine.reset();
  std::filesystem::remove_all(game);
}

TEST_CASE("the budget is game.json's script_budget_ms, else the engine's own") {
  const std::filesystem::path given = budget_game("given", budget_settings());
  CHECK(read_game_manifest(given).settings.script_budget_ms == kTestBudgetMs);
  const std::filesystem::path none = budget_game("none", "{}");
  CHECK(read_game_manifest(none).settings.script_budget_ms == config::kDefaultScriptBudgetMs);
  CHECK(read_game_manifest(none).script_budget_error.empty());
  for (const char* unusable : {"0", "-5", "1.5", "\"fast\"", "100000000"}) {
    const std::filesystem::path wrong = budget_game("wrong", std::string("{\"script_budget_ms\": ") + unusable + "}");
    const GameManifest manifest = read_game_manifest(wrong);
    CHECK(manifest.settings.script_budget_ms == config::kDefaultScriptBudgetMs);
    CHECK(manifest.script_budget_error.find("script_budget_ms must be a whole number") != std::string::npos);
    std::filesystem::remove_all(wrong);
  }
  std::filesystem::remove_all(given);
  std::filesystem::remove_all(none);
}

TEST_CASE("a stopped call's ensure runs and puts things back") {
  const TestDeadline deadline("resetter");
  const std::filesystem::path game = budget_game("ensure", budget_settings());
  std::unique_ptr<Engine> engine = started(game, scene_of({"resetter.rb"}));
  frames(*engine, 1);
  CHECK(engine->scripts().error_count() == 1);
  CHECK(ruby(*engine, "$state.inspect") == ":idle");
  engine.reset();
  std::filesystem::remove_all(game);
}

TEST_CASE("an ensure that never returns is stopped all the same") {
  check_stopped("endless_ensure.rb", "EndlessEnsure#update",
                "it went on in a rescue or an ensure after it was stopped");
}

TEST_CASE("objects a stopped call spawns as it unwinds get their behaviours, and the error names the culprit") {
  const TestDeadline deadline("spawner");
  const std::filesystem::path game = budget_game("spawner", budget_settings());
  test::LogCapture capture;
  std::unique_ptr<Engine> engine = started(game, scene_of({"spawner.rb", "innocent.rb"}));
  frames(*engine, kFramesAfter);
  CHECK(engine->scripts().error_count() == 1);
  CHECK(capture.contains("Spawner#update on 'spawner.rb' (behaviour disabled)"));
  CHECK(capture.contains("ScriptTimeout"));
  CHECK(ruby(*engine, "$innocents") == "2");  // the scene's, and the one spawned
  engine.reset();
  std::filesystem::remove_all(game);
}

TEST_CASE("the calls the engine makes from inside a call are not that call's time") {
  const TestDeadline deadline("patient");
  const std::filesystem::path game = budget_game("patient", budget_settings());
  test::LogCapture capture;
  std::unique_ptr<Engine> engine = started(game, scene_of({"patient.rb", "slow.rb"}));
  frames(*engine, kFramesAfter);
  CHECK(engine->scripts().error_count() == 0);
  CHECK(ruby(*engine, "$patient_done.inspect") == "true");
  engine.reset();
  std::filesystem::remove_all(game);
}

TEST_CASE("a running game's scripts are on the budget game.json gives them") {
  const std::filesystem::path game = budget_game("running", budget_settings());
  const SceneDesc scene = scene_of({"counter.rb"});
  std::unique_ptr<Engine> engine = started(game, scene);
  CHECK(engine->scripts().vm().budget() == kTestBudget);
  engine.reset();
  std::filesystem::remove_all(game);
}

TEST_CASE("an interpreter has no budget until given one, and each call gets all of it") {
  const TestDeadline deadline("interpreter");
  RubyVm vm;
  CHECK(vm.budget().count() == 0);
  vm.set_budget(kTestBudget);
  // Calls that each take most of the budget, longer than it together, all finish.
  const std::string most_of_it =
      "t = Time.now; while Time.now - t < " + std::to_string(kTestBudgetMs / 2 / 1000.0) + "; end; :done";
  for (int i = 0; i < kFramesAfter; ++i) CHECK(vm.eval(most_of_it, "most.rb").ok);
  const RubyResult stuck = vm.eval("loop {}", "stuck.rb");
  CHECK_FALSE(stuck.ok);
  CHECK(stuck.error.find("ScriptTimeout") != std::string::npos);
  // An exception that never finishes saying what it is does not hang the engine either.
  const RubyResult endless =
      vm.eval("class Endless < StandardError; def inspect = loop {}; end; raise Endless", "e.rb");
  CHECK_FALSE(endless.ok);
  CHECK(vm.eval("1", "after.rb").ok);
}

#if !defined(_WIN32)
namespace {

// The stack a thread gets on the Mac (where a server's rooms run on threads of their own).
constexpr std::size_t kSmallThreadStack = 512 * 1024;

// Recursion through the engine's C side (a block called from Array#sort, which is C), each level
// a call of the interpreter inside another: the C stack grows with it.
void* dive_through_c(void* result) {
  RubyVm vm;
  const RubyResult dived = vm.eval("def dive = [2, 1].sort { dive; 0 }\ndive", "dive.rb");
  *static_cast<std::string*>(result) = dived.ok ? "returned" : dived.error;
  return nullptr;
}

}  // namespace

TEST_CASE("a recursion through C stops with SystemStackError before a small thread's stack runs out") {
  const TestDeadline deadline("recursion through C");
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, kSmallThreadStack);
  std::string result;
  pthread_t thread;
  REQUIRE(pthread_create(&thread, &attributes, dive_through_c, &result) == 0);
  pthread_join(thread, nullptr);
  pthread_attr_destroy(&attributes);
  CHECK(result.find("SystemStackError") != std::string::npos);
}
#endif

TEST_CASE("a loop that only ever spawns objects is stopped all the same, within a few budgets") {
  const TestDeadline deadline("breeder");
  const std::filesystem::path game = budget_game("breeder", budget_settings());
  test::LogCapture capture;
  std::unique_ptr<Engine> engine = started(game, scene_of({"breeder.rb", "innocent.rb"}));
  CHECK(frames(*engine, 1) < kStoppedWell);
  CHECK(capture.contains("Breeder#update on 'breeder.rb' (behaviour disabled)"));
  CHECK(capture.contains("ScriptTimeout"));
  engine.reset();
  std::filesystem::remove_all(game);
}

namespace {

constexpr std::chrono::milliseconds kIdleWatch{400};  // eight of the budget below, with no call running
constexpr std::chrono::milliseconds kShortBudget{50};
constexpr std::uint64_t kMostIdleWakes = 2;  // its last look at the call, and the one after it

}  // namespace

TEST_CASE("the watchdog sleeps while no script runs") {
  const TestDeadline deadline("idle watchdog");
  RubyVm vm;
  vm.set_budget(kShortBudget);
  REQUIRE(vm.eval("1 + 1", "one.rb").ok);
  const std::uint64_t before = script_watchdog_wakes();
  std::this_thread::sleep_for(kIdleWatch);
  CHECK(script_watchdog_wakes() - before <= kMostIdleWakes);
  // ... and wakes when a call needs it.
  const RubyResult stuck = vm.eval("loop {}", "stuck.rb");
  CHECK(stuck.error.find("ScriptTimeout") != std::string::npos);
}
