#pragma once

#include <mruby.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

namespace redgem {

// How long one call from the engine into the scripts may run before it is stopped: a script that
// never returns (`loop {}`, `while true; end`) must not freeze the game, the editor or a room of
// a server. A call that runs past it is ended with a ScriptTimeout, which the engine treats as
// any other error (the behaviour it came from is disabled), and the game goes on.
//
// How a call is stopped:
// - A watchdog thread (one for the whole program) sets the interpreter's code fetch hook (the
//   function mruby calls before each instruction, here budget_hook) when the call running has
//   passed its deadline. Until then there is none, and a call pays nothing for it. (The engine's
//   mruby reads the hook atomically, cmake/patch_mruby.cmake, so setting it from another thread
//   is safe; nothing else of the interpreter is touched from there.)
// - The hook, on the interpreter's own thread, checks the clock itself and decides: a request
//   for a call that has since ended, or that is not overdue, is dropped.
// - The first time, the call is raised a ScriptTimeout once, where it stands, and has a short
//   grace (kScriptGraceMs) to unwind: its `ensure` clauses run. If it still runs after that (an
//   ensure that loops, a rescue that carries on), it is raised again at every instruction until
//   it has returned.
//
// What a call's time is: from when it begins, less the time spent in calls the engine makes from
// inside it (each has a budget of its own: a stopped call does not stop them) and in the engine's
// own work that is known to be long (Pause: spawning objects, placing scatter). A function of the
// engine is never stopped halfway: the call ends at its next instruction of Ruby.
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

  // A call from the engine begins and ends (on the interpreter's thread).
  void begin_call() { push(true); }
  void end_call() { pop(); }
  // Engine work known to be long begins and ends inside a call: its time is not the call's.
  void begin_pause() { push(false); }
  void end_pause() { pop(); }

  // The interpreter's look at its clock when the watchdog asks (from the hook), which may raise.
  void on_stop_request(mrb_state* mrb, const mrb_code* pc);

  // The watchdog's side: when the call running must be looked at (kNever when none runs, or it
  // runs the engine's work), and the request to look.
  static constexpr std::int64_t kNever = INT64_MAX;
  std::int64_t deadline() const { return deadline_.load(std::memory_order_seq_cst); }
  void request_stop();

 private:
  // What a call is to its budget: running on it, stopped once and unwinding on its grace, or
  // stopped for good.
  enum class Phase { Running, Unwinding, Stopped };
  struct Frame {
    bool limited = true;         // a call; else a pause
    std::int64_t started = 0;    // less the time of what ran inside it
    std::int64_t unwinding = 0;  // when it was stopped once, likewise
    std::int64_t left_at = 0;    // when what runs inside it began
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
  using Hook = void (*)(mrb_state*, const mrb_irep*, const mrb_code*, mrb_value*);
  void set_hook(Hook hook);
  void silence();

  mrb_state* mrb_ = nullptr;
  RClass* timeout_class_ = nullptr;
  std::atomic<long long> limit_ms_{0};
  std::vector<Frame> frames_;  // the calls in calls, innermost last (the interpreter's thread)
  std::atomic<std::int64_t> deadline_{kNever};
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
