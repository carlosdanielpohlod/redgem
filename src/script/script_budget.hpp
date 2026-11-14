#pragma once

#include <mruby.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

#include "core/clock.hpp"

namespace redgem {

class ScriptSampler;

// How long one call from the engine into the scripts may run before it is stopped: a script that
// never returns (`loop {}`, `while true; end`) must not freeze the game, the editor or a room of
// a server. A call that runs past it is ended with a ScriptTimeout, which the engine treats as
// any other error (the behaviour it came from is disabled), and the game goes on.
//
// How a call is stopped:
// - While a limit is set, the interpreter's code fetch hook (the function mruby calls before each
//   instruction) is budget_hook, set on the interpreter's own thread and never from another. All
//   it does is read an atomic flag.
// - A watchdog thread (one for the whole program) sets that flag when the call running has passed
//   its deadline. It touches nothing else: the interpreter's thread, seeing the flag, checks the
//   clock itself and decides (a request for a call that has since ended is dropped).
// - The first time, the call is raised a ScriptTimeout once, where it stands, and has a short
//   grace (kScriptGraceMs) to unwind: its `ensure` clauses run. If it still runs after that (an
//   ensure that loops, a rescue that carries on), it is raised again at every instruction until
//   it has returned.
//
// What a call's time is: from when it begins, less the time spent in calls the engine makes from
// inside it (each has a budget of its own: a stopped call does not stop them) and in the engine's
// own work that is known to be long (Pause: spawning objects, placing scatter, reading and writing
// the save and the settings), up to
// kScriptForgivenBudgets of its budgets in all: a loop that only ever spawns is stopped too. A
// function of the engine is never stopped halfway: the call ends at its next instruction of Ruby.
//
// What a call's time is read on: the computer's coarse clock (core/coarse_clock.hpp), or a clock
// given to the budget (a test's, which only moves when the test moves it). On a given clock the
// watchdog looks at a running call every kScriptWatchRetryMs of real time, and the call is past
// its deadline only once that clock says so: a call on a clock that does not move never is.
class ScriptBudget {
 public:
  using Milliseconds = std::chrono::milliseconds;

  ScriptBudget() = default;
  ~ScriptBudget();
  ScriptBudget(const ScriptBudget&) = delete;
  ScriptBudget& operator=(const ScriptBudget&) = delete;

  // The interpreter it watches, and the class of the error a call that ran too long ends with.
  // Before any limit is set.
  void attach(mrb_state* mrb, RClass* timeout_class);
  // No limit (zero) until one is set; on the interpreter's thread, between calls. Set back to
  // zero, the watchdog no longer looks at the interpreter (which can then be closed).
  void set_limit(Milliseconds limit);
  Milliseconds limit() const { return Milliseconds(limit_ms_.load(std::memory_order_relaxed)); }
  // The scripts' sampling profiler the hook feeds, or none (on the interpreter's thread, between
  // calls). The hook is set while there is a limit or a sampler.
  void set_sampler(ScriptSampler* sampler);
  ScriptSampler* sampler() const { return sampler_; }
  // The clock calls are timed on; none (the default) for the computer's coarse clock. On the
  // interpreter's thread, between calls; the clock outlives the budget's use of it.
  void set_clock(const Clock* clock) { clock_.store(clock, std::memory_order_seq_cst); }
  // Milliseconds on that clock.
  std::int64_t now() const;

  // A call from the engine begins and ends (on the interpreter's thread).
  void begin_call() { push(true); }
  void end_call() { pop(); }
  // Engine work known to be long begins and ends inside a call: its time is not the call's.
  void begin_pause() { push(false); }
  void end_pause() { pop(); }

  // Whether the watchdog asks the interpreter to look at its clock; and the look (on the
  // interpreter's thread, from the hook), which may raise.
  bool stop_requested() const { return stop_.load(std::memory_order_relaxed); }
  void on_stop_request(mrb_state* mrb, const mrb_code* pc);

  // The watchdog's side: when the call running must be looked at (kNever when none runs, or it
  // runs the engine's work), and the request to look.
  static constexpr std::int64_t kNever = INT64_MAX;
  std::int64_t deadline() const { return deadline_.load(std::memory_order_seq_cst); }
  // Whether the call running is past its deadline on the budget's clock.
  bool past_deadline() const { return deadline() <= now(); }
  // When, on the coarse clock (where `real_now` is), the watchdog is to look next: the deadline
  // itself, or on a clock of its own a short while from now while a call runs (kNever when none).
  std::int64_t next_look(std::int64_t real_now) const;
  void request_stop() { stop_.store(true, std::memory_order_relaxed); }

 private:
  // What a call is to its budget: running on it, stopped once and unwinding on its grace, or
  // stopped for good.
  enum class Phase { Running, Unwinding, Stopped };
  struct Frame {
    bool limited = true;         // a call; else a pause
    std::int64_t started = 0;    // less the time of what ran inside it
    std::int64_t unwinding = 0;  // when it was stopped once, likewise
    std::int64_t left_at = 0;    // when what runs inside it began
    std::int64_t forgiven = 0;   // of its time, how much was what ran inside it
    Phase phase = Phase::Running;
  };

  void push(bool limited);
  void pop();
  void resume(Frame& frame, std::int64_t now);
  // Whether the frame is a call, on an interpreter with a limit.
  bool counts(const Frame& frame) const;
  std::int64_t deadline_of(const Frame& frame) const;
  void publish(std::int64_t deadline);
  // Whether the call on top is past its deadline at `now`.
  bool overdue(const Frame& frame, std::int64_t now) const;
  void step_up(Frame& frame, std::int64_t now);
  void raise(mrb_state* mrb, const mrb_code* pc, const Frame& frame, std::int64_t now);
  void silence() { stop_.store(false, std::memory_order_relaxed); }
  void refresh_hook();
  // How much of `away` is not the frame's time: what is left of what it may be forgiven.
  std::int64_t forgivable(const Frame& frame, std::int64_t away) const;

  mrb_state* mrb_ = nullptr;
  RClass* timeout_class_ = nullptr;
  ScriptSampler* sampler_ = nullptr;
  std::atomic<long long> limit_ms_{0};
  std::vector<Frame> frames_;  // the calls in calls, innermost last (the interpreter's thread)
  std::atomic<std::int64_t> deadline_{kNever};
  std::atomic<bool> stop_{false};
  std::atomic<const Clock*> clock_{nullptr};  // (read by the watchdog too)
};

// How many times the watchdog has woken since the program started (for the tests of its sleep).
std::uint64_t script_watchdog_wakes();

// Counts a call into the scripts against the budget for as long as it lasts, with a budget of its
// own (see ScriptBudget): a call from C++, or a block the engine yields to from inside its work.
class BudgetCall {
 public:
  explicit BudgetCall(ScriptBudget& budget) : budget_(budget) { budget_.begin_call(); }
  ~BudgetCall() { budget_.end_call(); }
  BudgetCall(const BudgetCall&) = delete;
  BudgetCall& operator=(const BudgetCall&) = delete;

 private:
  ScriptBudget& budget_;
};

// Counts what runs inside it as engine work, not the call's (see ScriptBudget).
class BudgetPause {
 public:
  explicit BudgetPause(ScriptBudget& budget) : budget_(budget) { budget_.begin_pause(); }
  ~BudgetPause() { budget_.end_pause(); }
  BudgetPause(const BudgetPause&) = delete;
  BudgetPause& operator=(const BudgetPause&) = delete;

 private:
  ScriptBudget& budget_;
};

// mruby's code fetch hook, for an interpreter whose allocator data is its ScriptBudget.
void budget_hook(mrb_state* mrb, const mrb_irep* irep, const mrb_code* pc, mrb_value* regs);

}  // namespace redgem
