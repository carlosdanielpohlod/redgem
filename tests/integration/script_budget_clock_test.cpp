// The scripts' time budget on a clock a test moves by hand: whether a call is past its budget is
// that clock's to say, never how fast the machine running the test is.

#include <doctest/doctest.h>

#include <mruby.h>

#include <chrono>
#include <cstdint>
#include <string>

#include "fake_clock.hpp"
#include "script_ticks.hpp"
#include "script/ruby_vm.hpp"
#include "script/script_budget.hpp"
#include "script/script_config.hpp"

using namespace redgem;

namespace {

constexpr int kBudgetMs = 20;
constexpr std::chrono::milliseconds kBudget{kBudgetMs};
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr double kBudgetSeconds = kBudgetMs / kMillisecondsPerSecond;
constexpr int kTicksPerBudget = 4;
// Engine work of three budgets, under the four a call is forgiven: once is forgiven, twice is not.
constexpr double kLongWorkBudgets = 3.0;
// Real time a call spins for: several budgets, which on the test's clock is no time at all.
constexpr double kRealSpinBudgets = 5.0;

// (What the natives below move; the test that defines them sets it.)
test::FakeClock* moved_clock = nullptr;
RubyVm* budgeted_vm = nullptr;

// stall: three budgets pass in the engine's code that is not known to be long.
mrb_value stall(mrb_state*, mrb_value) {
  moved_clock->advance(kLongWorkBudgets * kBudgetSeconds);
  return mrb_nil_value();
}

// long_work: three budgets pass in engine work known to be long (BudgetPause).
mrb_value long_work(mrb_state*, mrb_value) {
  const BudgetPause pause(budgeted_vm->budget_keeper());
  moved_clock->advance(kLongWorkBudgets * kBudgetSeconds);
  return mrb_nil_value();
}

// An interpreter with a budget, timed on a fake clock, with `tick` (a quarter of a budget) and the
// natives above.
struct ClockedVm {
  test::FakeClock clock;
  RubyVm vm;
  test::ScriptTicks ticks{vm, clock, kBudgetSeconds / kTicksPerBudget};

  ClockedVm() {
    moved_clock = &clock;
    budgeted_vm = &vm;
    vm.set_budget(kBudget);
    mrb_state* mrb = vm.state();
    mrb_define_method(mrb, mrb->kernel_module, "stall", stall, MRB_ARGS_NONE());
    mrb_define_method(mrb, mrb->kernel_module, "long_work", long_work, MRB_ARGS_NONE());
  }
  ~ClockedVm() {
    vm.set_budget(std::chrono::milliseconds(0));
    moved_clock = nullptr;
    budgeted_vm = nullptr;
  }
  ClockedVm(const ClockedVm&) = delete;
  ClockedVm& operator=(const ClockedVm&) = delete;
};

// Whether a call ended because it ran past its budget.
bool timed_out(const RubyResult& result) {
  return !result.ok && result.error.find("ScriptTimeout") != std::string::npos;
}

}  // namespace

TEST_CASE("the budget reads the clock it is given: real time spent is not a call's time on it") {
  ClockedVm clocked;
  const std::string spin = "t = Time.now; nil while Time.now - t < " +
                           std::to_string(kRealSpinBudgets * kBudgetSeconds) + "; :done";
  CHECK(clocked.vm.eval(spin, "spin.rb").ok);
  CHECK(clocked.vm.budget_keeper().now() ==
        static_cast<std::int64_t>(test::kFakeClockStart * kMillisecondsPerSecond));
}

TEST_CASE("a call is stopped once the given clock has passed its budget, and not before") {
  ClockedVm clocked;
  const RubyResult stopped = clocked.vm.eval("$ticks = 0; loop { $ticks += 1; tick }", "ticks.rb");
  CHECK(timed_out(stopped));
  CHECK(mrb_test(clocked.vm.eval("$ticks >= " + std::to_string(kTicksPerBudget), "count.rb").value));
  // A long stall outside known engine work is the call's time: stopped at its next Ruby.
  CHECK(timed_out(clocked.vm.eval("stall; loop {}", "stall.rb")));
  // The next call has a budget of its own.
  CHECK(clocked.vm.eval("40 + 2", "after.rb").ok);
}

TEST_CASE("engine work known to be long is forgiven on the given clock, up to a point") {
  ClockedVm clocked;
  static_assert(kLongWorkBudgets < config::kScriptForgivenBudgets);
  CHECK(clocked.vm.eval("long_work; :done", "once.rb").ok);
  // Past the budgets a call is forgiven, it is stopped as it comes back from the work.
  static_assert(2 * kLongWorkBudgets > config::kScriptForgivenBudgets + 1);
  CHECK(timed_out(clocked.vm.eval("long_work; long_work; :done", "twice.rb")));
}
