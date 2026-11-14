// Saving is the engine's time, not the script's: a disk slower than the scripts' budget for a
// moment does not stop the call that saves. Timed on a fake clock that the disk moves.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "fake_clock.hpp"
#include "script/ruby_vm.hpp"
#include "script/script_config.hpp"
#include "storage/save_file.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr double kMillisecondsPerSecond = 1000.0;
// How long one file written to the disk takes on the test's clock: more than a whole budget
// alone, and two of them (a save and the backup of the one before) under the budgets a call is
// forgiven.
constexpr double kSlowWriteBudgets = 1.5;
static_assert(2 * kSlowWriteBudgets < config::kScriptForgivenBudgets);
// Real time a call goes on after it saved: many looks of the watchdog, which would have stopped it
// by then had the write been its time.
constexpr double kWatchedSeconds = 20 * config::kScriptWatchRetryMs / kMillisecondsPerSecond;
constexpr int kTicksPerBudget = 4;

test::FakeClock* moved_clock = nullptr;
double budget_seconds = 0.0;

// tick: a quarter of a budget passes.
mrb_value tick(mrb_state*, mrb_value) {
  moved_clock->advance(budget_seconds / kTicksPerBudget);
  return mrb_nil_value();
}

// A game whose save is kept in a file on a slow disk, its scripts timed on a fake clock.
struct SlowDiskGame {
  std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-save-budget-test";
  test::FakeClock clock;
  std::unique_ptr<SaveFile> save;
  std::unique_ptr<Engine> engine;

  SlowDiskGame() {
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    save = std::make_unique<SaveFile>(folder / "save.json", [this](std::FILE*) { return slow_write(); });
    EngineOptions options;
    options.game_dir = test::fixture("games/sandbox");
    options.save = save.get();
    options.fixed_step = true;
    auto made = Engine::create(options);
    REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(made));
    engine = std::move(std::get<std::unique_ptr<Engine>>(made));
    time_on_clock();
  }
  ~SlowDiskGame() {
    engine.reset();
    save.reset();
    moved_clock = nullptr;
    std::filesystem::remove_all(folder);
  }
  SlowDiskGame(const SlowDiskGame&) = delete;
  SlowDiskGame& operator=(const SlowDiskGame&) = delete;

  void time_on_clock() {
    RubyVm& vm = engine->scripts().vm();
    REQUIRE(vm.budget().count() > 0);
    budget_seconds = static_cast<double>(vm.budget().count()) / kMillisecondsPerSecond;
    moved_clock = &clock;
    vm.budget_keeper().set_clock(&clock);
    mrb_define_method(vm.state(), vm.state()->kernel_module, "tick", tick, MRB_ARGS_NONE());
  }

  bool slow_write() {
    clock.advance(kSlowWriteBudgets * budget_seconds);
    return true;
  }

  RubyResult run(const std::string& code) { return engine->scripts().eval(code, "save_budget.rb"); }
  std::string text(const RubyResult& result) {
    REQUIRE_MESSAGE(result.ok, result.error);
    return RSTRING_PTR(mrb_obj_as_string(engine->scripts().vm().state(), result.value));
  }
};

// Ruby that goes on for kWatchedSeconds of real time, then gives `value`.
std::string then_watched(const std::string& value) {
  return "; t = Time.now; nil while Time.now - t < " + std::to_string(kWatchedSeconds) + "; " + value;
}

}  // namespace

TEST_CASE("a write of the save slower than the budget does not stop the call that saves") {
  SlowDiskGame game;
  CHECK(game.text(game.run("Save[:coins] = 7" + then_watched("Save[:coins]"))) == "7");
  CHECK(game.clock.now() > test::kFakeClockStart + budget_seconds);
  CHECK(game.text(game.run("Save.delete(:coins)" + then_watched("Save[:coins].inspect"))) == "nil");
}

TEST_CASE("Save.update's slow write is the engine's time, and its block a call of its own") {
  SlowDiskGame game;
  CHECK(game.text(game.run("Save.update(:coins, 1) { |coins| coins + 1 }" + then_watched("Save[:coins]"))) == "2");
  const RubyResult stuck = game.run("Save.update(:coins, 0) { loop { tick } }");
  CHECK_FALSE(stuck.ok);
  CHECK(stuck.error.find("ScriptTimeout") != std::string::npos);
  CHECK(game.text(game.run("Save[:coins]")) == "2");
}
